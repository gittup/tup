#define _GNU_SOURCE
#define _ATFILE_SOURCE
#include "parser.h"
#include "list.h"
#include "flist.h"
#include "fileio.h"
#include "fslurp.h"
#include "db.h"
#include "memdb.h"
#include "vardb.h"
#include "graph.h"
#include "config.h"
#include "bin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

struct name_list {
	struct list_head entries;
	int num_entries;
	int totlen;
	int extlesstotlen;
	int basetotlen;
	int extlessbasetotlen;
};

struct name_list_entry {
	struct list_head list;
	char *path;
	char *base;
	int len;
	int extlesslen;
	int baselen;
	int extlessbaselen;
	tupid_t tupid;
	tupid_t dt;
	int type;
};

struct path_list {
	struct list_head list;
	/* For files: */
	char *path;
	const char *file;
	tupid_t dt;
	/* For bins: */
	struct bin *bin;
};

struct rule {
	struct list_head list;
	int foreach;
	char *input_pattern;
	char *output_pattern;
	struct bin *bin;
	char *command;
	struct name_list namelist;
	int line_number;
};

struct build_name_list_args {
	struct name_list *nl;
	const char *dir;
	int dirlen;
};

static int parse_tupfile(struct buf *b, struct vardb *vdb, tupid_t tupid,
			 tupid_t curdir, struct graph *g);
static int parse_rule(char *p, tupid_t tupid, int lno, struct graph *g,
		      struct vardb *vdb, struct bin_list *bl);
static int parse_varsed(char *p, tupid_t tupid, int lno, struct graph *g,
			struct vardb *vdb, struct bin_list *bl);
static int parse_bin(char *p, struct bin_list *bl, struct bin **b, int lno);
static int execute_rule(struct rule *r, struct graph *g, struct vardb *vdb,
			tupid_t tupid, struct bin_list *bl);
static int parse_input_patterns(char *p, tupid_t dt, struct name_list *nl,
				struct graph *g, struct bin_list *bl, int lno);
static int get_path_list(char *p, struct list_head *plist, tupid_t dt,
			 struct bin_list *bl, struct list_head *symlist);
static int parse_dependent_tupfiles(struct list_head *plist, tupid_t dt,
				    struct graph *g);
static int get_name_list(struct list_head *plist, struct name_list *nl);
static int nl_add_path(struct path_list *pl, struct name_list *nl);
static int nl_add_bin(struct bin *b, tupid_t dt, struct name_list *nl);
static int build_name_list_cb(void *arg, struct db_node *dbn);
static char *set_path(const char *name, const char *dir, int dirlen);
static int do_rule(struct rule *r, struct name_list *nl, struct name_list *oonl,
		   struct vardb *vdb, tupid_t tupid);
static void init_name_list(struct name_list *nl);
static void set_nle_base(struct name_list_entry *nle);
static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle);
static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle);
static char *tup_printf(const char *cmd, struct name_list *nl,
			struct name_list *onl);
static char *eval(struct vardb *v, const char *string, tupid_t tupid);

int parse(struct node *n, struct graph *g)
{
	int dfd;
	int fd;
	int rc = -1;
	tupid_t parent;
	int num_dotdots;
	struct buf b;
	struct vardb vdb;

	if(n->parsing) {
		fprintf(stderr, "Error: Circular dependency found among Tupfiles (last dir ID %lli  = '%s').\nThis is madness!\n", n->tupid, n->name);
		return -1;
	}
	n->parsing = 1;

	if(vardb_init(&vdb) < 0)
		return -1;
	parent = n->tupid;
	num_dotdots = 0;
	while(parent != DOT_DT) {
		tupid_t new;
		new = tup_db_parent(parent);
		if(new < 0) {
			fprintf(stderr, "Error finding parent node of node ID %lli. Database might be hosed.\n", parent);
			return -1;
		}
		parent = new;
		num_dotdots++;
	}
	if(num_dotdots) {
		/* No +1 because we leave off the trailing '/' */
		char *path = malloc(num_dotdots * 3);
		char *p;
		if(!path) {
			perror("malloc");
			return -1;
		}
		p = path;
		for(; num_dotdots; num_dotdots--) {
			strcpy(p, "..");
			p += 2;
			if(num_dotdots > 1) {
				strcpy(p, "/");
				p++;
			}
		}
		if(vardb_set(&vdb, "TUP_TOP", path) < 0)
			return -1;
		free(path);
	} else {
		if(vardb_set(&vdb, "TUP_TOP", ".") < 0)
			return -1;
	}

	/* Move all existing commands over to delete - then the ones that are
	 * re-created will be moved back out to modify or none when parsing the
	 * Tupfile. All those that are no longer generated remain in delete for
	 * cleanup.
	 *
	 * Also delete links to our directory from directories that we depend
	 * on. These will be re-generated when the file is parsed, or when
	 * the database is rolled back in case of error.
	 */
	if(tup_db_flag_delete_in_dir(n->tupid, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_flag_delete_cmd_outputs(n->tupid) < 0)
		return -1;
	if(tup_db_remove_output_links(n->tupid) < 0)
		return -1;
	if(tup_db_delete_dependent_dir_links(n->tupid) < 0)
		return -1;

	dfd = tup_db_open_tupid(n->tupid);
	if(dfd < 0) {
		fprintf(stderr, "Error: Unable to open directory ID %lli\n", n->tupid);
		goto out_close_vdb;
	}

	fd = openat(dfd, "Tupfile", O_RDONLY);
	/* No Tupfile means we have nothing to do */
	if(fd < 0) {
		rc = 0;
		goto out_close_dfd;
	}

	if((rc = fslurp(fd, &b)) < 0) {
		goto out_close_file;
	}
	rc = parse_tupfile(&b, &vdb, n->tupid, n->tupid, g);
	free(b.s);
out_close_file:
	close(fd);
out_close_dfd:
	close(dfd);
out_close_vdb:
	if(vardb_close(&vdb) < 0)
		rc = -1;

	if(rc == 0) {
		if(tup_db_set_flags_by_id(n->tupid, TUP_FLAGS_NONE) < 0)
			return -1;
	}

	return rc;
}

static int parse_tupfile(struct buf *b, struct vardb *vdb, tupid_t tupid,
			 tupid_t curdir, struct graph *g)
{
	char *p, *e;
	char *line;
	int if_true = 1;
	int lno = 0;
	int linelen;
	struct bin_list bl;

	if(bin_list_init(&bl) < 0)
		return -1;

	p = b->s;
	e = b->s + b->len;

	while(p < e) {
		char *newline;

		/* Skip leading whitespace and empty lines */
		while(p < e && isspace(*p)) {
			if(*p == '\n')
				lno++;
			p++;
		}
		/* If we just had empty lines at the end, we're done */
		if(p == e)
			break;

		line = p;
		newline = strchr(p, '\n');
		if(!newline)
			goto syntax_error;
		lno++;
		while(newline[-1] == '\\') {
			newline[-1] = ' ';
			newline[0] = ' ';
			newline = strchr(p, '\n');
			if(!newline)
				goto syntax_error;
			lno++;
		}

		*newline = 0;
		p = newline + 1;

		linelen = newline - line;

		if(line[0] == '#') {
			/* Skip comments */
			continue;
		}

		if(strncmp(line, "include ", 8) == 0 ||
		   strncmp(line, "include_root ", 13) == 0) {
			struct buf incb;
			int fd;
			int rc;
			int include_root = (line[7] == '_');
			char *file;
			struct name_list nl;
			struct name_list_entry *nle, *tmpnle;
			struct path_list *pl, *tmppl;
			int sym_bork = 0;
			tupid_t incdir;
			LIST_HEAD(plist);
			LIST_HEAD(symlist);

			init_name_list(&nl);

			if(include_root) {
				file = line + 13;
				incdir = DOT_DT;
			} else {
				file = line + 8;
				incdir = curdir;
			}
			file = eval(vdb, file, tupid);
			if(!file)
				return -1;
			if(get_path_list(file, &plist, incdir, NULL, &symlist) < 0)
				return -1;
			while(!list_empty(&symlist)) {
				struct half_entry *he;

				he = list_entry(symlist.next, struct half_entry, list);
				if(he->type == TUP_NODE_GENERATED) {
					fprintf(stderr, "Error: Attempted to include a Tupfile from a path that contains a symlink generated by tup. Directory %lli, symlink used was %lli, line number %i\n", tupid, he->tupid, lno);
					sym_bork = 1;
				}
				list_del(&he->list);
				free(he);
			}
			if(sym_bork)
				return -1;
			if(get_name_list(&plist, &nl) < 0)
				return -1;
			list_for_each_entry_safe(pl, tmppl, &plist, list) {
				list_del(&pl->list);
				free(pl);
			}
			/* Can only be freed after plist */
			free(file);
			list_for_each_entry_safe(nle, tmpnle, &nl.entries, list) {
				if(nle->type != TUP_NODE_FILE) {
					if(nle->type == TUP_NODE_GENERATED) {
						fprintf(stderr, "Error: Unable to include generated file '%s'. Your build configuration must be comprised of files you wrote yourself.\n", nle->path);
						return -1;
					} else {
						fprintf(stderr, "tup error: Attempt to include node (ID %lli, name='%s') of type %i?\n", nle->tupid, nle->path, nle->type);
						return -1;
					}
				}
				fd = tup_db_open_tupid(nle->tupid);
				if(fd < 0) {
					fprintf(stderr, "Error including '%s': %s\n", nle->path, strerror(errno));
					return -1;
				}
				rc = fslurp(fd, &incb);
				close(fd);
				if(rc < 0) {
					fprintf(stderr, "Error slurping file.\n");
					return -1;
				}

				/* When parsing the included Tupfile, any files
				 * it includes will be relative to it
				 * (nle->dt), not to the parent dir (tupid).
				 * However, we want all links to be made to the
				 * parent tupid.
				 */
				rc = parse_tupfile(&incb, vdb, tupid, nle->dt,
						   g);
				free(incb.s);
				if(rc < 0) {
					fprintf(stderr, "Error parsing included file '%s'\n", line);
					return -1;
				}

				if(tup_db_create_link(nle->tupid, tupid, TUP_LINK_NORMAL) < 0)
					return -1;
				delete_name_list_entry(&nl, nle);
			}
		} else if(strcmp(line, "else") == 0) {
			if_true = !if_true;
		} else if(strcmp(line, "endif") == 0) {
			/* TODO: Nested if */
			if_true = 1;
		} else if(!if_true) {
			/* Skip the false part of an if block */
		} else if(strncmp(line, "ifeq ", 5) == 0) {
			char *paren;
			char *comma;
			char *lval;
			char *rval;

			paren = strchr(line+5, '(');
			if(!paren)
				goto syntax_error;
			lval = paren + 1;
			comma = strchr(lval, ',');
			if(!comma)
				goto syntax_error;
			rval = comma + 1;

			paren = line + linelen;
			while(paren > line) {
				if(*paren == ')')
					goto found_paren;
				paren--;
			}
			goto syntax_error;
found_paren:
			*comma = 0;
			*paren = 0;

			lval = eval(vdb, lval, tupid);
			if(!lval)
				return -1;
			rval = eval(vdb, rval, tupid);
			if(!rval)
				return -1;
			if(strcmp(lval, rval) == 0) {
				if_true = 1;
			} else {
				if_true = 0;
			}
			free(lval);
			free(rval);
		} else if(line[0] == ':') {
			if(parse_rule(line+1, tupid, lno, g, vdb, &bl) < 0)
				goto syntax_error;
		} else if(line[0] == ',') {
			if(parse_varsed(line+1, tupid, lno, g, vdb, &bl) < 0)
				goto syntax_error;
		} else {
			char *eq;
			char *var;
			char *value;
			int append;

			/* Find the += or = sign, and point value to the start
			 * of the string after that op.
			 */
			eq = strstr(line, "+=");
			if(eq) {
				value = eq + 2;
				append = 1;
			} else {
				eq = strstr(line, ":=");
				if(eq) {
					value = eq + 2;
					append = 0;
				} else {
					eq = strchr(line, '=');
					if(!eq)
						goto syntax_error;
					value = eq + 1;
					append = 0;
				}
			}

			/* End the lval with a 0, then space-delete the end
			 * of the variable name and the beginning of the value.
			 */
			*eq = 0;
			while(isspace(*value) && *value != 0)
				value++;
			eq--;
			while(isspace(*eq) && eq > line) {
				*eq = 0;
				eq--;
			}

			var = eval(vdb, line, tupid);
			if(!var)
				return -1;
			value = eval(vdb, value, tupid);
			if(!value)
				return -1;

			if(append)
				vardb_append(vdb, var, value);
			else
				vardb_set(vdb, var, value);
			free(var);
			free(value);
		}
	}

	bin_list_del(&bl);

	return 0;

syntax_error:
	fprintf(stderr, "Syntax error parsing Tupfile line %i\n  Line was: '%s'\n", lno, line);
	return -1;
}

static int parse_rule(char *p, tupid_t tupid, int lno, struct graph *g,
		      struct vardb *vdb, struct bin_list *bl)
{
	char *input, *cmd, *output;
	char *ie, *ce;
	struct rule r;
	char empty[] = "";
	int rc;

	input = p;
	while(isspace(*input))
		input++;
	p = strstr(p, "|>");
	if(!p)
		return -1;
	if(input < p) {
		ie = p - 1;
		while(isspace(*ie))
			ie--;
		ie[1] = 0;
	} else {
		input = NULL;
	}
	p += 2;
	cmd = p;
	while(isspace(*cmd))
		cmd++;
	p = strstr(p, "|>");
	if(!p)
		return -1;
	ce = p - 1;
	while(isspace(*ce))
		ce--;
	p += 2;
	output = p;
	while(isspace(*output))
		output++;
	ce[1] = 0;
	if(parse_bin(p, bl, &r.bin, lno) < 0)
		return -1;
	/* Don't rely on p now, since parse_bin fiddles with things */

	r.foreach = 0;
	if(input) {
		if(strncmp(input, "foreach", 7) == 0) {
			r.foreach = 1;
			input += 7;
			while(*input == ' ') input++;
		}
		r.input_pattern = input;
	} else {
		r.input_pattern = empty;
	}
	r.output_pattern = output;
	r.command = cmd;
	r.line_number = lno;
	init_name_list(&r.namelist);

	rc = execute_rule(&r, g, vdb, tupid, bl);
	return rc;
}

static int parse_varsed(char *p, tupid_t tupid, int lno, struct graph *g,
			struct vardb *vdb, struct bin_list *bl)
{
	char *input, *output;
	char *ie;
	int rc;
	struct rule r;
	char command[] = ", %f > %o";

	input = p;
	while(isspace(*input))
		input++;
	p = strstr(p, "|>");
	if(!p)
		return -1;
	if(input < p) {
		ie = p - 1;
		while(isspace(*ie))
			ie--;
		ie[1] = 0;
	} else {
		input = NULL;
	}
	p += 2;
	output = p;
	while(isspace(*output))
		output++;
	if(parse_bin(p, bl, &r.bin, lno) < 0)
		return -1;
	/* Don't rely on p now, since parse_bin fiddles with things */

	r.foreach = 1;
	r.input_pattern = input;
	r.output_pattern = output;
	r.command = command;
	r.line_number = lno;
	init_name_list(&r.namelist);

	rc = execute_rule(&r, g, vdb, tupid, bl);
	return rc;
}

static int parse_bin(char *p, struct bin_list *bl, struct bin **b, int lno)
{
	char *oe;
	char *bin;

	*b = NULL;

	p = strchr(p, '[');
	/* No bin is ok */
	if(!p)
		return 0;

	/* Terminate the real end of the output list */
	oe = p - 1;
	while(isspace(*oe))
		oe--;
	oe[1] = 0;

	bin = p+1;
	p = strchr(p, ']');
	if(!p) {
		fprintf(stderr, "Parse error line %i: Expecting end ']' for bin name.\n", lno);
		return -1;
	}
	*p = 0;

	/* Bin must be at the end of the line */
	if(p[1] != 0) {
		fprintf(stderr, "Parse error line %i: Trailing characters after output bin: '%s'\n", lno, p+1);
		return -1;
	}

	*b = bin_add(bin, bl);
	if(!*b)
		return -1;
	return 0;
}

static int execute_rule(struct rule *r, struct graph *g, struct vardb *vdb,
			tupid_t tupid, struct bin_list *bl)
{
	char *oosep;
	struct name_list order_only_nl;
	struct name_list_entry *nle;
	char *input_pattern;

	init_name_list(&order_only_nl);

	input_pattern = eval(vdb, r->input_pattern, tupid);
	if(!input_pattern)
		return -1;
	oosep = strchr(input_pattern, '|');
	if(oosep) {
		char *p = oosep;
		*p = 0;
		while(p >= input_pattern && isspace(*p)) {
			*p = 0;
			p--;
		}
		oosep++;
		while(*oosep && isspace(*oosep)) {
			*oosep = 0;
			oosep++;
		}
		if(parse_input_patterns(oosep, tupid, &order_only_nl, g, bl, r->line_number) < 0)
			return -1;
	}
	if(parse_input_patterns(input_pattern, tupid, &r->namelist, g, bl, r->line_number) < 0)
		return -1;

	if(r->foreach) {
		struct name_list tmp_nl;
		struct name_list_entry tmp_nle;

		/* For a foreach loop, iterate over each entry in the rule's
		 * namelist and do a shallow copy over into a single-entry
		 * temporary namelist. Note that we cheat by not actually
		 * allocating a separate nle, which is why we don't have to do
		 * a delete_name_list_entry for the temporary list and can just
		 * reinitialize the pointers using init_name_list.
		 */
		while(!list_empty(&r->namelist.entries)) {
			nle = list_entry(r->namelist.entries.next,
					 struct name_list_entry, list);
			init_name_list(&tmp_nl);
			memcpy(&tmp_nle, nle, sizeof(*nle));
			add_name_list_entry(&tmp_nl, &tmp_nle);
			if(do_rule(r, &tmp_nl, &order_only_nl, vdb, tupid) < 0)
				return -1;

			delete_name_list_entry(&r->namelist, nle);
		}
	}

	/* Only parse non-foreach rules if the namelist has some entries, or if
	 * there is no input listed. We don't want to generate a command if
	 * there is an input pattern but no entries match (for example, *.o
	 * inputs to ld %f with no object files). However, if you have no input
	 * but just a command (eg: you want to run a shell script), then we
	 * still want to do the rule for that case.
	 *
	 * Also note that we check that the original user string is empty
	 * (r->input_pattern), not the eval'd string (input_pattern). This way
	 * if the user specifies the input as $(foo) and it evaluates to empty,
	 * we won't try to execute do_rule(). But an empty user string implies
	 * that no input is required.
	 */
	if(!r->foreach && (r->namelist.num_entries > 0 ||
			   strcmp(r->input_pattern, "") == 0)) {

		if(do_rule(r, &r->namelist, &order_only_nl, vdb, tupid) < 0)
			return -1;

		while(!list_empty(&r->namelist.entries)) {
			nle = list_entry(r->namelist.entries.next,
					 struct name_list_entry, list);
			delete_name_list_entry(&r->namelist, nle);
		}
	}

	while(!list_empty(&order_only_nl.entries)) {
		nle = list_entry(order_only_nl.entries.next,
				 struct name_list_entry, list);
		delete_name_list_entry(&order_only_nl, nle);
	}

	free(input_pattern);
	return 0;
}

static int parse_input_patterns(char *p, tupid_t dt, struct name_list *nl,
				struct graph *g, struct bin_list *bl, int lno)
{
	LIST_HEAD(plist);
	LIST_HEAD(symlist);
	struct path_list *pl, *tmp;
	int sym_bork = 0;

	if(get_path_list(p, &plist, dt, bl, &symlist) < 0)
		return -1;
	while(!list_empty(&symlist)) {
		struct half_entry *he;

		he = list_entry(symlist.next, struct half_entry, list);
		if(he->type == TUP_NODE_GENERATED) {
			fprintf(stderr, "Error: Attempted to input files using a symlink that was generated by tup. Directory %lli, symlink used was %lli, line number %i\n", dt, he->tupid, lno);
			sym_bork = 1;
		}
		list_del(&he->list);
		free(he);
	}
	if(sym_bork)
		return -1;
	if(parse_dependent_tupfiles(&plist, dt, g) < 0)
		return -1;
	if(get_name_list(&plist, nl) < 0)
		return -1;
	list_for_each_entry_safe(pl, tmp, &plist, list) {
		list_del(&pl->list);
		free(pl);
	}
	return 0;
}

static int get_path_list(char *p, struct list_head *plist, tupid_t dt,
			 struct bin_list *bl, struct list_head *symlist)
{
	struct path_list *pl;
	int spc_index;
	int last_entry = 0;

	do {
		spc_index = strcspn(p, " \t");
		if(p[spc_index] == 0)
			last_entry = 1;
		p[spc_index] = 0;
		if(spc_index == 0)
			goto skip_empty_space;

		pl = malloc(sizeof *pl);
		if(!pl) {
			perror("malloc");
			return -1;
		}
		pl->path = NULL;
		pl->file = NULL;
		pl->dt = 0;
		pl->bin = NULL;

		if(p[0] == '[') {
			/* Bin */
			char *endb;

			if(!bl) {
				fprintf(stderr, "Parse error: Bins are only usable in an input or output list.\n");
				return -1;
			}

			endb = strchr(p, ']');
			if(!endb) {
				fprintf(stderr, "Parse error: Expecting end bracket for input bin.\n");
				return -1;
			}
			*endb = 0;
			p++;
			pl->bin = bin_find(p, bl);
			if(!pl->bin) {
				fprintf(stderr, "Parse error: Unable to find bin '%s'\n", p);
				return -1;
			}
		} else {
			/* Path */
			pl->path = p;
			pl->dt = find_dir_tupid_dt(dt, p, &pl->file, symlist, 0);
			if(pl->dt <= 0) {
				fprintf(stderr, "Error: Failed to find directory ID for dir '%s' relative to %lli\n", p, dt);
				return -1;
			}
			if(pl->path == pl->file) {
				pl->path = NULL;
			} else {
				/* File points to somewhere later in the path,
				 * so set the last '/' to 0.
				 */
				pl->path[pl->file-pl->path-1] = 0;
			}
		}
		list_add_tail(&pl->list, plist);

skip_empty_space:
		p += spc_index + 1;
	} while(!last_entry);

	return 0;
}

static int parse_dependent_tupfiles(struct list_head *plist, tupid_t dt,
				    struct graph *g)
{
	struct path_list *pl;

	list_for_each_entry(pl, plist, list) {
		if(pl->dt != dt) {
			struct node *n;
			if(memdb_find(&g->memdb, pl->dt, &n) < 0)
				return -1;
			if(n != NULL && !n->already_used) {
				n->already_used = 1;
				if(parse(n, g) < 0)
					return -1;
			}
			if(tup_db_create_link(pl->dt, dt, TUP_LINK_NORMAL) < 0)
				return -1;
		}
	}
	return 0;
}

static int get_name_list(struct list_head *plist, struct name_list *nl)
{
	struct path_list *pl;

	list_for_each_entry(pl, plist, list) {
		if(pl->bin) {
			if(nl_add_bin(pl->bin, pl->dt, nl) < 0)
				return -1;
		} else {
			if(nl_add_path(pl, nl) < 0)
				return -1;
		}
	}
	return 0;
}

static int nl_add_path(struct path_list *pl, struct name_list *nl)
{
	struct build_name_list_args args;

	if(pl->path != NULL) {
		/* Note that dirlen should be pl->file - pl->path - 1,
		 * but we add 1 to account for the trailing '/' that
		 * will be added.
		 */
		args.dir = pl->path;
		args.dirlen = pl->file-pl->path;
	} else {
		args.dir = "";
		args.dirlen = 0;
	}
	args.nl = nl;
	if(strchr(pl->file, '*') == NULL) {
		struct db_node dbn;
		int rc;
		if(tup_db_select_dbn(pl->dt, pl->file, &dbn) < 0) {
			return -1;
		}
		if(dbn.tupid < 0) {
			fprintf(stderr, "Error: Explicitly named file '%s' not found in subdir %lli.\n", pl->file, pl->dt);
			return -1;
		}
		if(dbn.type == TUP_NODE_GHOST) {
			fprintf(stderr, "Error: Explicitly named file '%s' is a ghost file, so it can't be used as an input.\n", pl->file);
			return -1;
		}
		rc = tup_db_in_delete_list(dbn.tupid);
		if(rc < 0)
			return -1;
		if(rc == 1) {
			fprintf(stderr, "Error: Explicitly named file '%s' in subdir %lli is scheduled to be deleted (possibly the command that created it has been removed).\n", pl->file, pl->dt);
			return -1;
		}
		if(build_name_list_cb(&args, &dbn) < 0)
			return -1;
	} else {
		if(tup_db_select_node_dir_glob(build_name_list_cb, &args, pl->dt, pl->file) < 0)
			return -1;
	}
	return 0;
}

static int nl_add_bin(struct bin *b, tupid_t dt, struct name_list *nl)
{
	struct bin_entry *be;
	struct name_list_entry *nle;
	int extlesslen;

	list_for_each_entry(be, &b->entries, list) {
		extlesslen = be->len - 1;
		while(extlesslen > 0 && be->name[extlesslen] != '.')
			extlesslen--;
		if(extlesslen == 0)
			extlesslen = be->len;

		nle = malloc(sizeof *nle);
		if(!nle) {
			perror("malloc");
			return -1;
		}

		nle->path = malloc(be->len + 1);
		if(!nle->path)
			return -1;
		memcpy(nle->path, be->name, be->len+1);

		/* All binned nodes are generated from commands */
		nle->len = be->len;
		nle->extlesslen = extlesslen;
		nle->tupid = be->tupid;
		nle->dt = dt;
		nle->type = TUP_NODE_GENERATED;
		set_nle_base(nle);

		add_name_list_entry(nl, nle);
	}
	return 0;
}

static int build_name_list_cb(void *arg, struct db_node *dbn)
{
	struct build_name_list_args *args = arg;
	int extlesslen;
	int len;
	int namelen;
	struct name_list_entry *nle;

	if(tup_db_in_delete_list(dbn->tupid))
		return 0;

	namelen = strlen(dbn->name);
	len = namelen + args->dirlen;
	extlesslen = namelen - 1;
	while(extlesslen > 0 && dbn->name[extlesslen] != '.')
		extlesslen--;
	if(extlesslen == 0)
		extlesslen = namelen;
	extlesslen += args->dirlen;

	nle = malloc(sizeof *nle);
	if(!nle) {
		perror("malloc");
		return -1;
	}

	nle->path = set_path(dbn->name, args->dir, args->dirlen);
	if(!nle->path)
		return -1;

	nle->len = len;
	nle->extlesslen = extlesslen;
	nle->tupid = dbn->tupid;
	nle->dt = dbn->dt;
	nle->type = dbn->type;
	set_nle_base(nle);

	add_name_list_entry(args->nl, nle);
	return 0;
}

static char *set_path(const char *name, const char *dir, int dirlen)
{
	char *path;

	if(dirlen) {
		int nlen;

		nlen = strlen(name);
		/* +1 for '/', +1 for nul */
		path = malloc(nlen + dirlen + 1);
		if(!path) {
			perror("malloc");
			return NULL;
		}

		memcpy(path, dir, dirlen-1);
		path[dirlen-1] = '/';
		strcpy(path + dirlen, name);
	} else {
		path = strdup(name);
		if(!path) {
			perror("strdup");
			return NULL;
		}
	}
	return path;
}

static int do_rule(struct rule *r, struct name_list *nl, struct name_list *oonl,
		   struct vardb *vdb, tupid_t tupid)
{
	struct name_list onl;
	struct name_list_entry *nle, *onle;
	char *output_pattern;
	char *tcmd;
	char *cmd;
	struct path_list *pl;
	int rc;
	tupid_t cmd_id;
	LIST_HEAD(oplist);

	init_name_list(&onl);

	output_pattern = eval(vdb, r->output_pattern, tupid);
	if(!output_pattern)
		return -1;
	if(get_path_list(output_pattern, &oplist, tupid, NULL, NULL) < 0)
		return -1;
	while(!list_empty(&oplist)) {
		pl = list_entry(oplist.next, struct path_list, list);
		if(pl->path) {
			fprintf(stderr, "Error: Attempted to create an output file '%s', which contains a '/' character. Tupfiles should only output files in their own directories.\n - Directory: %lli\n - Rule at line %i: [35m%s[0m\n", pl->path, tupid, r->line_number, r->command);
			return -1;
		}
		onle = malloc(sizeof *onle);
		if(!onle) {
			perror("malloc");
			return -1;
		}
		onle->path = tup_printf(pl->file, nl, NULL);
		if(!onle->path)
			return -1;
		onle->len = strlen(onle->path);
		onle->extlesslen = onle->len - 1;
		while(onle->extlesslen > 0 && onle->path[onle->extlesslen] != '.')
			onle->extlesslen--;

		onle->tupid = tup_db_create_node_part(tupid, onle->path, -1,
						      TUP_NODE_GENERATED);
		if(onle->tupid < 0)
			return -1;

		add_name_list_entry(&onl, onle);

		if(r->bin) {
			if(bin_add_entry(r->bin, onle->path, onle->len, onle->tupid) < 0)
				return -1;
		}

		list_del(&pl->list);
		free(pl);
	}
	/* Has to be freed after use of oplist */
	free(output_pattern);

	tcmd = tup_printf(r->command, nl, &onl);
	if(!tcmd)
		return -1;
	cmd = eval(vdb, tcmd, tupid);
	if(!cmd)
		return -1;
	free(tcmd);

	cmd_id = create_command_file(tupid, cmd);
	free(cmd);
	if(cmd_id < 0)
		return -1;

	if(tup_db_unsticky_links(cmd_id) < 0)
		return -1;

	while(!list_empty(&onl.entries)) {
		onle = list_entry(onl.entries.next, struct name_list_entry,
				  list);
		/* Note: If this link becomes sticky, be careful that var/sed
		 * commands still work (since they don't go through the normal
		 * server process to create links).
		 */
		if(tup_db_create_unique_link(cmd_id, onle->tupid) < 0) {
			fprintf(stderr, "You may have multiple commands trying to create file '%s'\n", onle->path);
			return -1;
		}
		delete_name_list_entry(&onl, onle);
	}

	list_for_each_entry(nle, &nl->entries, list) {
		if(tup_db_create_link(nle->tupid, cmd_id, TUP_LINK_STICKY) < 0)
			return -1;
	}
	list_for_each_entry(nle, &oonl->entries, list) {
		if(tup_db_create_link(nle->tupid, cmd_id, TUP_LINK_STICKY) < 0)
			return -1;
	}
	if(tup_db_delete_empty_links(cmd_id) < 0)
		return -1;
	rc = tup_db_yell_links(cmd_id, "Missing a required input file. If you removed an input file from a rule  that isn't needed anymore, you should be able to remove it after a successful   update. Another possibility is a command is now writing to a node that was      previously a ghost.");
	if(rc < 0)
		return -1;
	if(rc == 0)
		return 0;
	return -1;
}

static void init_name_list(struct name_list *nl)
{
	INIT_LIST_HEAD(&nl->entries);
	nl->num_entries = 0;
	nl->totlen = 0;
	nl->extlesstotlen = 0;
	nl->basetotlen = 0;
	nl->extlessbasetotlen = 0;
}

static void set_nle_base(struct name_list_entry *nle)
{
	nle->base = nle->path + nle->len;
	nle->baselen = 0;
	while(nle->base > nle->path) {
		nle->base--;
		if(nle->base[0] == '/') {
			nle->base++;
			goto out;
		}
		nle->baselen++;
	}
out:
	/* The extension-less baselen is the length of the base, minus the
	 * length of the extension we calculated before.
	 */
	nle->extlessbaselen = nle->baselen - (nle->len - nle->extlesslen);
}

static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle)
{
	list_add_tail(&nle->list, &nl->entries);
	nl->num_entries++;
	nl->totlen += nle->len;
	nl->extlesstotlen += nle->extlesslen;
	nl->basetotlen += nle->baselen;
	nl->extlessbasetotlen += nle->extlessbaselen;
}

static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle)
{
	nl->num_entries--;
	nl->totlen -= nle->len;
	nl->extlesstotlen -= nle->extlesslen;
	nl->basetotlen -= nle->baselen;
	nl->extlessbasetotlen -= nle->extlessbaselen;

	list_del(&nle->list);
	free(nle->path);
	free(nle);
}

static char *tup_printf(const char *cmd, struct name_list *nl,
			struct name_list *onl)
{
	struct name_list_entry *nle;
	char *s;
	int x;
	const char *p;
	const char *next;
	const char *spc;
	int clen = strlen(cmd);

	p = cmd;
	while((p = strchr(p, '%')) !=  NULL) {
		int paste_chars;

		clen -= 2;
		p++;
		spc = p + 1;
		while(*spc && *spc != ' ')
			spc++;
		paste_chars = (nl->num_entries - 1) * (spc - p);
		if(*p == 'f') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%f used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->totlen + paste_chars;
		} else if(*p == 'F') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%F used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->extlesstotlen + paste_chars;
		} else if(*p == 'b') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%b used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->basetotlen + paste_chars;
		} else if(*p == 'B') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%B used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->extlessbasetotlen + paste_chars;
		} else if(*p == 'o') {
			if(!onl) {
				fprintf(stderr, "Error: %%o can only be used in a command.\n");
				return NULL;
			}
			if(onl->num_entries == 0) {
				fprintf(stderr, "Error: %%o used in rule pattern and no output files were specified.\n");
				return NULL;
			}
			clen += onl->totlen + (onl->num_entries-1);
		}
	}

	s = malloc(clen + 1);
	if(!s) {
		perror("malloc");
		return NULL;
	}

	p = cmd;
	x = 0;
	while((next = strchr(p, '%')) !=  NULL) {
		memcpy(&s[x], p, next-p);
		x += next-p;

		next++;
		p = next + 1;
		spc = p;
		while(*spc && *spc != ' ')
			spc++;
		if(*next == 'f') {
			int first = 1;
			list_for_each_entry(nle, &nl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->path, nle->len);
				x += nle->len;
				memcpy(&s[x], p, spc - p);
				x += spc - p;
				first = 0;
			}
		} else if(*next == 'F') {
			int first = 1;
			list_for_each_entry(nle, &nl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->path, nle->extlesslen);
				x += nle->extlesslen;
				memcpy(&s[x], p, spc - p);
				x += spc - p;
				first = 0;
			}
		} else if(*next == 'b') {
			int first = 1;
			list_for_each_entry(nle, &nl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->base, nle->baselen);
				x += nle->baselen;
				memcpy(&s[x], p, spc - p);
				x += spc - p;
				first = 0;
			}
		} else if(*next == 'B') {
			int first = 1;
			list_for_each_entry(nle, &nl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->base, nle->extlessbaselen);
				x += nle->extlessbaselen;
				memcpy(&s[x], p, spc - p);
				x += spc - p;
				first = 0;
			}
		} else if(*next == 'o') {
			int first = 1;
			list_for_each_entry(nle, &onl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->path, nle->len);
				x += nle->len;
				first = 0;
			}
			memcpy(&s[x], p, spc - p);
			x += spc - p;
		}
		p = spc;
	}
	strcpy(&s[x], p);
	if((signed)strlen(s) != clen) {
		fprintf(stderr, "Error: Calculated string length (%i) didn't match actual (%li). String is: '%s'.\n", clen, (long)strlen(s), s);
		return NULL;
	}
	return s;
}

static char *eval(struct vardb *v, const char *string, tupid_t tupid)
{
	int len = 0;
	char *ret;
	char *p;
	const char *s;
	const char *var;
	const char *expected = "oops";
	int vlen;

	s = string;
	while(*s) {
		if(*s == '\\') {
			if((s[1] == '$' && s[2] == '(') ||
			   s[1] == '@') {
				/* \$( becomes $( */
				/* \@ becomes @ */
				len++;
				s += 2;
			} else {
				len++;
				s++;
			}
		} else if(*s == '$') {
			const char *rparen;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					expected = "ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				vlen = vardb_len(v, var, rparen-var);
				if(vlen < 0)
					return NULL;
				len += vlen;
				s = rparen + 1;
			} else {
				s++;
				len++;
			}
		} else if(*s == '@') {
			const char *rat;

			rat = strchr(s+1, '@');
			if(!rat) {
				expected = "ending @-symbol";
				goto syntax_error;
			}

			var = s + 1;
			vlen = tup_db_get_varlen(var, rat-s-1);
			if(vlen < 0)
				return NULL;
			len += vlen;
			s = rat + 1;
		} else {
			s++;
			len++;
		}
	}

	ret = malloc(len+1);
	if(!ret) {
		perror("malloc");
		return NULL;
	}

	p = ret;
	s = string;
	while(*s) {
		if(*s == '\\') {
			if((s[1] == '$' && s[2] == '(') ||
			   s[1] == '@') {
				/* \$( becomes $( */
				/* \@ becomes @ */
				*p = s[1];
				s += 2;
			} else {
				*p = *s;
				s++;
			}
			p++;
		} else if(*s == '$') {
			const char *rparen;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					expected = "ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				if(vardb_get(v, var, rparen-var, &p) < 0)
					return NULL;

				s = rparen + 1;
			} else {
				*p = *s;
				p++;
				s++;
			}
		} else if(*s == '@') {
			const char *rat;
			tupid_t vt;

			rat = strchr(s+1, '@');
			if(!rat) {
				expected = "ending @-symbol";
				goto syntax_error;
			}

			var = s + 1;
			vt = tup_db_get_var(var, rat-s-1, &p);
			if(vt < 0)
				return NULL;
			if(tup_db_create_link(vt, tupid, TUP_LINK_NORMAL) < 0)
				return NULL;

			s = rat + 1;
		} else {
			*p = *s;
			p++;
			s++;
		}
	}
	strcpy(p, s);

	if((signed)strlen(ret) != len) {
		fprintf(stderr, "Length mismatch: expected %i bytes, wrote %li\n", len, (long)strlen(ret));
		return NULL;
	}

	return ret;

syntax_error:
	fprintf(stderr, "Syntax error: expected %s\n", expected);
	return NULL;
}
