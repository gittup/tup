#define _GNU_SOURCE
#define _ATFILE_SOURCE
#include "parser.h"
#include "linux/list.h"
#include "flist.h"
#include "fileio.h"
#include "fslurp.h"
#include "db.h"
#include "vardb.h"
#include "graph.h"
#include "config.h"
#include "bin.h"
#include "dirtree.h"
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
	int dirlen;
	tupid_t tupid;
	tupid_t dt;
	int type;
};

struct path_list {
	struct list_head list;
	/* For files: */
	char *path;
	struct path_element *pel;
	tupid_t dt;
	/* For bins: */
	struct bin *bin;
};

struct rule {
	struct list_head list;
	int foreach;
	char *output_pattern;
	struct bin *bin;
	char *command;
	struct name_list inputs;
	struct name_list order_only_inputs;
	int empty_input;
	int line_number;
};

struct build_name_list_args {
	struct tupfile *tf;
	struct name_list *nl;
	const char *dir;
	int dirlen;
};

struct tupfile {
	tupid_t tupid;
	int dfd;
	struct graph *g;
	struct vardb vdb;
	struct rb_root cmd_tree;
	int ign;
};

static int parse_tupfile(struct tupfile *tf, struct buf *b, tupid_t curdir,
			 const char *cwd, int clen);
static int include_rules(struct tupfile *tf, tupid_t curdir,
			 const char *cwd, int clen);
static int gitignore(struct tupfile *tf);
static int include_name_list(struct tupfile *tf, struct name_list *nl,
			     const char *cwd, int clen);
static int parse_rule(struct tupfile *tf, char *p, int lno, struct bin_list *bl,
		      const char *cwd, int clen);
static int parse_varsed(struct tupfile *tf, char *p, int lno,
			struct bin_list *bl, const char *cwd, int clen);
static int parse_bang_definition(struct tupfile *tf, char *p, int lno);
static int parse_bang_rule(struct tupfile *tf, struct rule *r, void **mem,
			   const char *cwd, int clen);
static int parse_bin(char *p, struct bin_list *bl, struct bin **b, int lno);
static int split_input_pattern(char *p, char **o_input, char **o_cmd,
			       char **o_output, char **o_bin);
static int parse_input_pattern(struct tupfile *tf, char *input_pattern,
			       struct name_list *inputs,
			       struct name_list *order_only_inputs,
			       struct bin_list *bl, int lno,
			       const char *cwd, int clen);
static int execute_rule(struct tupfile *tf, struct rule *r,
			const char *cwd, int clen);
static int input_pattern_to_nl(struct tupfile *tf, char *p,
			       struct name_list *nl, struct bin_list *bl,
			       int lno);
static int get_path_list(char *p, struct list_head *plist, tupid_t dt,
			 struct bin_list *bl, struct list_head *symlist);
static int parse_dependent_tupfiles(struct list_head *plist, tupid_t dt,
				    struct graph *g);
static int get_name_list(struct tupfile *tf, struct list_head *plist,
			 struct name_list *nl);
static int nl_add_path(struct tupfile *tf, struct path_list *pl,
		       struct name_list *nl);
static int nl_add_bin(struct bin *b, tupid_t dt, struct name_list *nl);
static int build_name_list_cb(void *arg, struct db_node *dbn);
static char *set_path(const char *name, const char *dir, int dirlen);
static int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
		   const char *cwd, int clen, const char *ext, int extlen);
static void init_name_list(struct name_list *nl);
static void set_nle_base(struct name_list_entry *nle);
static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle);
static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle);
static char *tup_printf(const char *cmd, int cmd_len, struct name_list *nl,
			struct name_list *onl, const char *ext, int extlen);
static char *eval(struct tupfile *tf, const char *string,
		  const char *cwd, int clen);

int parse(struct node *n, struct graph *g)
{
	struct tupfile tf;
	int fd;
	int rc = -1;
	struct buf b;

	if(n->parsing) {
		fprintf(stderr, "Error: Circular dependency found among Tupfiles (last dir ID %lli  = '%s').\nThis is madness!\n", n->tnode.tupid, n->name);
		return -1;
	}
	n->parsing = 1;

	tf.tupid = n->tnode.tupid;
	tf.g = g;
	tf.cmd_tree.rb_node = NULL;
	tf.ign = 0;
	if(vardb_init(&tf.vdb) < 0)
		return -1;

	/* Move all existing commands over to delete - then the ones that are
	 * re-created will be moved back out to modify or none when parsing the
	 * Tupfile. All those that are no longer generated remain in delete for
	 * cleanup.
	 *
	 * Also delete links to our directory from directories that we depend
	 * on. These will be re-generated when the file is parsed, or when
	 * the database is rolled back in case of error.
	 */
	if(tup_db_cmds_to_tree(tf.tupid, &g->delete_tree, &g->delete_count) < 0)
		return -1;
	if(tup_db_cmd_outputs_to_tree(tf.tupid, &g->delete_tree, &g->delete_count) < 0)
		return -1;
	if(tup_db_delete_dependent_dir_links(tf.tupid) < 0)
		return -1;
	if(tup_db_delete_gitignore(tf.tupid, &g->delete_tree, &g->delete_count) < 0)
		return -1;

	tf.dfd = dirtree_open(tf.tupid);
	if(tf.dfd < 0) {
		fprintf(stderr, "Error: Unable to open directory ID %lli\n", tf.tupid);
		goto out_close_vdb;
	}

	fd = openat(tf.dfd, "Tupfile", O_RDONLY);
	/* No Tupfile means we have nothing to do */
	if(fd < 0) {
		rc = 0;
		goto out_close_dfd;
	}

	if((rc = fslurp(fd, &b)) < 0) {
		goto out_close_file;
	}
	rc = parse_tupfile(&tf, &b, tf.tupid, ".", 1);
	free(b.s);
	if(tf.ign) {
		if(gitignore(&tf) < 0)
			rc = -1;
	}
out_close_file:
	close(fd);
out_close_dfd:
	close(tf.dfd);
out_close_vdb:
	if(vardb_close(&tf.vdb) < 0)
		rc = -1;
	free_tupid_tree(&tf.cmd_tree);

	return rc;
}

static int parse_tupfile(struct tupfile *tf, struct buf *b, tupid_t curdir,
			 const char *cwd, int clen)
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

		if(strncmp(line, "include ", 8) == 0) {
			char *file;
			struct name_list nl;
			struct path_list *pl, *tmppl;
			int sym_bork = 0;
			LIST_HEAD(plist);
			LIST_HEAD(symlist);

			init_name_list(&nl);

			file = line + 8;
			file = eval(tf, file, NULL, 0);
			if(!file)
				return -1;
			if(get_path_list(file, &plist, curdir, NULL, &symlist) < 0)
				return -1;
			while(!list_empty(&symlist)) {
				struct half_entry *he;

				he = list_entry(symlist.next, struct half_entry, list);
				if(he->type == TUP_NODE_GENERATED) {
					fprintf(stderr, "Error: Attempted to include a Tupfile from a path that contains a symlink generated by tup. Directory %lli, symlink used was %lli, line number %i\n", tf->tupid, he->tupid, lno);
					sym_bork = 1;
				}
				list_del(&he->list);
				free(he);
			}
			if(sym_bork)
				return -1;
			if(get_name_list(tf, &plist, &nl) < 0)
				return -1;
			list_for_each_entry_safe(pl, tmppl, &plist, list) {
				list_del(&pl->list);
				free(pl);
			}
			/* Can only be freed after plist */
			free(file);
			if(include_name_list(tf, &nl, cwd, clen) < 0)
				return -1;
		} else if(strcmp(line, "include_rules") == 0) {
			if(include_rules(tf, curdir, cwd, clen) < 0)
				return -1;
		} else if(strcmp(line, ".gitignore") == 0) {
			if(tf->ign) {
				fprintf(stderr, "Warning: .gitignore already specified earlier in the Tupfile (line %i is redundant).\n", lno);
			}
			tf->ign = 1;
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

			lval = eval(tf, lval, NULL, 0);
			if(!lval)
				return -1;
			rval = eval(tf, rval, NULL, 0);
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
			if(parse_rule(tf, line+1, lno, &bl, cwd, clen) < 0)
				goto syntax_error;
		} else if(line[0] == ',') {
			if(parse_varsed(tf, line+1, lno, &bl, cwd, clen) < 0)
				goto syntax_error;
		} else if(line[0] == '!') {
			if(parse_bang_definition(tf, line, lno) < 0)
				goto syntax_error;
		} else {
			char *eq;
			char *var;
			char *value;
			int append;
			int rc;

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

			var = eval(tf, line, cwd, clen);
			if(!var)
				return -1;
			value = eval(tf, value, cwd, clen);
			if(!value)
				return -1;

			if(append)
				rc = vardb_append(&tf->vdb, var, value);
			else
				rc = vardb_set(&tf->vdb, var, value);
			if(rc < 0) {
				fprintf(stderr, "Error setting variable '%s'\n", var);
				return -1;
			}
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

static int include_rules(struct tupfile *tf, tupid_t curdir,
			 const char *cwd, int clen)
{
	tupid_t parent;
	int num_dotdots;
	int x;
	char *p;
	char *path;
	int plen;
	char tuprules[] = "Tuprules.tup";
	int trlen = sizeof(tuprules) - 1;
	struct name_list nl;
	struct build_name_list_args args;

	parent = curdir;
	num_dotdots = 0;
	while(parent != DOT_DT) {
		struct dirtree *dirt;

		if(dirtree_add(parent, &dirt) < 0)
			return -1;
		if(!dirt || !dirt->parent) {
			fprintf(stderr, "Error finding parent node of ID %lli. Database might be hosed.\n", parent);
			return -1;
		}
		parent = dirt->parent->tnode.tupid;
		num_dotdots++;
	}
	path = malloc(num_dotdots * 3 + trlen + 1);
	if(!path) {
		perror("malloc");
		return -1;
	}
	p = path;
	for(x=0; x<num_dotdots; x++) {
		strcpy(p, "../");
		p += 3;
	}
	strcpy(path + num_dotdots*3, tuprules);
	/* Plen only includes the length of the path */
	plen = num_dotdots*3;

	init_name_list(&nl);
	args.tf = tf;
	args.nl = &nl;

	p = path;
	for(x=0; x<=num_dotdots; x++, p+=3, plen-=3) {
		struct db_node dbn;
		tupid_t dt;
		struct path_element *pel = NULL;
		LIST_HEAD(sym_list);

		dt = find_dir_tupid_dt(curdir, p, &pel, &sym_list, 0);
		if(dt < 0) {
			fprintf(stderr, "Error: Unable to find directory for '%s' relative to dir %lli\n", p, curdir);
			return -1;
		}
		if(!pel) {
			fprintf(stderr, "[31mtup internal error: didn't get a final pel pointer in include_rules()[0m\n");
			return -1;
		}
		if(tup_db_select_dbn_part(dt, pel->path, pel->len, &dbn) < 0) {
			return -1;
		}
		free(pel);
		if(dbn.tupid < 0) {
			/* Tuprules.tup doesn't exist here, go to the next
			 * dir.
			 */
			dbn.tupid = tup_db_node_insert(dt, tuprules, -1, TUP_NODE_GHOST, -1);
			if(dbn.tupid < 0)
				return -1;
			dbn.type = TUP_NODE_GHOST;

			/* Fall through to next if */
		}
		if(dbn.type == TUP_NODE_GHOST) {
			if(tup_db_create_link(dbn.tupid, tf->tupid, TUP_LINK_NORMAL) < 0)
				return -1;
			continue;
		}

		args.dir = p;
		args.dirlen = plen;
		if(build_name_list_cb(&args, &dbn) < 0)
			return -1;
	}
	free(path);
	return include_name_list(tf, &nl, cwd, clen);
}

static void tree_entry_remove(struct graph *g, tupid_t tupid)
{
	struct tree_entry *te;
	struct tupid_tree *tt;

	tt = tupid_tree_search(&g->delete_tree, tupid);
	if(!tt)
		return;
	rb_erase(&tt->rbn, &g->delete_tree);
	te = container_of(tt, struct tree_entry, tnode);
	if(te->type == TUP_NODE_GENERATED)
		g->delete_count--;
	free(te);
}

static int gitignore(struct tupfile *tf)
{
	char *s;
	int len;
	int fd;
	struct stat buf;
	int git_root = 0;

	if(fstatat(tf->dfd, ".git", &buf, 0) == 0) {
		if(S_ISDIR(buf.st_mode))
			git_root = 1;
	}

	if(tup_db_alloc_generated_nodelist(&s, &len, tf->tupid, &tf->g->delete_tree) < 0)
		return -1;
	if((s && len) || git_root == 1 || tf->tupid == 1) {
		struct db_node dbn;

		if(tup_db_select_dbn(tf->tupid, ".gitignore", &dbn) < 0)
			return -1;
		if(dbn.tupid < 0) {
			if(tup_db_node_insert(tf->tupid, ".gitignore", -1, TUP_NODE_GENERATED, -1) < 0)
				return -1;
		} else {
			tree_entry_remove(tf->g, dbn.tupid);
		}

		fd = openat(tf->dfd, ".gitignore", O_CREAT|O_WRONLY|O_TRUNC, 0666);
		if(fd < 0) {
			perror(".gitignore");
			return -1;
		}
		if(tf->tupid == 1) {
			write(fd, ".tup\n", 5);
		}
		if(git_root == 1) {
			write(fd, ".*.swp\n", 7);
			write(fd, ".gitignore\n", 11);
		}
		if(s && len) {
			write(fd, s, len);
		}
		close(fd);
	}
	if(s) {
		free(s); /* Freeze gopher! */
	}
	return 0;
}

static int include_name_list(struct tupfile *tf, struct name_list *nl,
			     const char *cwd, int clen)
{
	struct name_list_entry *nle, *tmpnle;
	struct buf incb;
	int fd;
	int rc;

	list_for_each_entry_safe(nle, tmpnle, &nl->entries, list) {
		const char *cnc;
		char *newcwd = NULL;
		int newclen;

		if(nle->type != TUP_NODE_FILE) {
			if(nle->type == TUP_NODE_GENERATED) {
				fprintf(stderr, "Error: Unable to include generated file '%s'. Your build configuration must be comprised of files you wrote yourself.\n", nle->path);
				return -1;
			} else {
				fprintf(stderr, "tup error: Attempt to include node (ID %lli, name='%s') of type %i?\n", nle->tupid, nle->path, nle->type);
				return -1;
			}
		}

		if(nle->dirlen) {
			/* Just make a quick check to get rid of the annoying
			 * leading "./" that would pop up.
			 */
			if(clen == 1) {
				/* Remove trailing slash */
				newclen = nle->dirlen - 1;
				newcwd = malloc(newclen + 1);
				if(!newcwd) {
					perror("malloc");
					return -1;
				}
				memcpy(newcwd, nle->path, nle->dirlen-1);
				newcwd[newclen] = 0;
			} else {
				/* Dirlen includes room for a trailing
				 * slash, which we move in between the
				 * two strings.
				 */
				newclen = clen + nle->dirlen;
				newcwd = malloc(newclen + 1);
				if(!newcwd) {
					perror("malloc");
					return -1;
				}
				memcpy(newcwd, cwd, clen);
				newcwd[clen] = '/';
				memcpy(newcwd+clen+1, nle->path, nle->dirlen-1);
				newcwd[newclen] = 0;
			}
			cnc = newcwd;
		} else {
			cnc = cwd;
			newclen = clen;
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
		rc = parse_tupfile(tf, &incb, nle->dt, cnc, newclen);
		free(incb.s);
		if(newcwd)
			free(newcwd);
		if(rc < 0) {
			fprintf(stderr, "Error parsing included file '%s'\n", nle->path);
			return -1;
		}

		if(tup_db_create_link(nle->tupid, tf->tupid, TUP_LINK_NORMAL) < 0)
			return -1;
		delete_name_list_entry(nl, nle);
	}
	return 0;
}

static int parse_rule(struct tupfile *tf, char *p, int lno, struct bin_list *bl,
		      const char *cwd, int clen)
{
	char *input, *cmd, *output, *bin;
	void *bang_mem = NULL;
	struct rule r;
	int rc;

	if(split_input_pattern(p, &input, &cmd, &output, &bin) < 0)
		return -1;
	if(bin) {
		if((r.bin = bin_add(bin, bl)) == NULL)
			return -1;
	} else {
		r.bin = NULL;
	}

	r.foreach = 0;
	if(input) {
		if(strncmp(input, "foreach", 7) == 0) {
			r.foreach = 1;
			input += 7;
			while(*input == ' ') input++;
		}
		r.empty_input = 0;
	} else {
		r.empty_input = 1;
	}
	r.output_pattern = output;
	r.command = cmd;
	r.line_number = lno;
	init_name_list(&r.inputs);
	init_name_list(&r.order_only_inputs);

	if(parse_input_pattern(tf, input, &r.inputs, &r.order_only_inputs, bl, r.line_number, cwd, clen) < 0)
		return -1;

	if(cmd[0] == '!') {
		if(parse_bang_rule(tf, &r, &bang_mem, cwd, clen) < 0)
			return -1;
	}

	rc = execute_rule(tf, &r, cwd, clen);
	if(bang_mem)
		free(bang_mem);
	return rc;
}

static int parse_varsed(struct tupfile *tf, char *p, int lno,
			struct bin_list *bl, const char *cwd, int clen)
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
	r.output_pattern = output;
	r.command = command;
	r.line_number = lno;
	r.empty_input = 0;
	init_name_list(&r.inputs);
	init_name_list(&r.order_only_inputs);

	if(parse_input_pattern(tf, input, &r.inputs, &r.order_only_inputs, bl, r.line_number, cwd, clen) < 0)
		return -1;

	rc = execute_rule(tf, &r, cwd, clen);
	return rc;
}

static int parse_bang_definition(struct tupfile *tf, char *p, int lno)
{
	char *eq;
	char *value;

	eq = strchr(p, '=');
	if(!eq) {
		fprintf(stderr, "Parse error line %i: Expecting '=' to set the bang rule.\n", lno);
		return -1;
	}
	value = eq + 1;
	while(*value && isspace(*value))
		value++;
	eq--;
	while(isspace(*eq))
		eq--;
	eq[1] = 0;

	if(vardb_set(&tf->vdb, p, value) < 0) {
		fprintf(stderr, "Error setting variable: '%s'\n", p);
		return -1;
	}
	return 0;
}

static int parse_bang_rule(struct tupfile *tf, struct rule *r, void **mem,
			   const char *cwd, int clen)
{
	const char *value;
	char *bangtext;
	char *input;
	char *cmd;
	char *output;
	char *bin;

	value = vardb_get(&tf->vdb, r->command);
	if(!value) {
		fprintf(stderr, "Error finding bang variable: '%s'\n", r->command);
		return -1;
	}

	bangtext = strdup(value);
	if(!bangtext) {
		perror("strdup");
		return -1;
	}

	/* Must be freed after execute_rule */
	*mem = bangtext;

	if(split_input_pattern(bangtext, &input, &cmd, &output, &bin) < 0)
		return -1;
	if(bin != NULL) {
		fprintf(stderr, "Error: bins aren't allowed in bang rules. Rule was: %s = %s\n", r->command, value);
		return -1;
	}

	/* Add any order only inputs to the list */
	if(parse_input_pattern(tf, input, NULL, &r->order_only_inputs, NULL,
			       r->line_number, cwd, clen) < 0)
		return -1;

	/* The command gets replaced whole-sale */
	r->command = cmd;

	/* Output only makes sense if one of them specifies it */
	if(r->output_pattern[0] && output[0]) {
		fprintf(stderr, "Error: Both the bang macro and rule specify output patterns.\n");
		return -1;
	}
	if(!r->output_pattern[0])
		r->output_pattern = output;
	return 0;
}

static int parse_bin(char *p, struct bin_list *bl, struct bin **b, int lno)
{
	char *oe;
	char *bin;

	*b = NULL;

	p = strchr(p, '{');
	/* No bin is ok */
	if(!p)
		return 0;

	/* Terminate the real end of the output list */
	oe = p - 1;
	while(isspace(*oe))
		oe--;
	oe[1] = 0;

	bin = p+1;
	p = strchr(p, '}');
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

static int split_input_pattern(char *p, char **o_input, char **o_cmd,
			       char **o_output, char **o_bin)
{
	char *input;
	char *cmd;
	char *output;
	char *bin = NULL;
	char *brace;
	char *ie, *ce, *oe;

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

	brace = strchr(output, '{');
	if(brace) {
		oe = brace - 1;
		while(oe > output && isspace(*oe))
			oe--;
		oe[1] = 0;
		bin = brace + 1;

		brace = strchr(bin, '}');
		if(!brace) {
			fprintf(stderr, "Missing '}' to finish bin\n");
			return -1;
		}
		if(brace[1]) {
			fprintf(stderr, "Error: bin must be at the end of the line\n");
			return -1;
		}
		*brace = 0;
	}

	*o_input = input;
	*o_cmd = cmd;
	*o_output = output;
	*o_bin = bin;
	return 0;
}

static int parse_input_pattern(struct tupfile *tf, char *input_pattern,
			       struct name_list *inputs,
			       struct name_list *order_only_inputs,
			       struct bin_list *bl, int lno,
			       const char *cwd, int clen)
{
	char *eval_pattern;
	char *oosep;

	if(!input_pattern)
		return 0;

	eval_pattern = eval(tf, input_pattern, cwd, clen);
	if(!eval_pattern)
		return -1;
	oosep = strchr(eval_pattern, '|');
	if(oosep) {
		char *p = oosep;
		*p = 0;
		while(p >= eval_pattern && isspace(*p)) {
			*p = 0;
			p--;
		}
		oosep++;
		while(*oosep && isspace(*oosep)) {
			*oosep = 0;
			oosep++;
		}
		if(input_pattern_to_nl(tf, oosep, order_only_inputs, bl, lno) < 0)
			return -1;
	}
	if(inputs) {
		if(input_pattern_to_nl(tf, eval_pattern, inputs, bl, lno) < 0)
			return -1;
	} else {
		if(eval_pattern[0]) {
			fprintf(stderr, "Error: bang rules can't have normal inputs, only order-only inputs. Pattern was: %s\n", input_pattern);
			return -1;
		}
	}
	free(eval_pattern);
	return 0;
}

static int execute_rule(struct tupfile *tf, struct rule *r,
			const char *cwd, int clen)
{
	struct name_list_entry *nle;

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
		while(!list_empty(&r->inputs.entries)) {
			nle = list_entry(r->inputs.entries.next,
					 struct name_list_entry, list);
			const char *ext = NULL;
			int extlen = 0;

			init_name_list(&tmp_nl);
			memcpy(&tmp_nle, nle, sizeof(*nle));
			add_name_list_entry(&tmp_nl, &tmp_nle);
			if(tmp_nle.base &&
			   tmp_nle.extlessbaselen != tmp_nle.baselen) {
				ext = tmp_nle.base + tmp_nle.extlessbaselen + 1;
				extlen = tmp_nle.baselen - tmp_nle.extlessbaselen - 1;
			}
			if(do_rule(tf, r, &tmp_nl, cwd, clen, ext, extlen) < 0)
				return -1;

			delete_name_list_entry(&r->inputs, nle);
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
	 * (r->empty_input), not the eval'd string. This way if the user
	 * specifies the input as $(foo) and it evaluates to empty, we won't
	 * try to execute do_rule(). But an empty user string implies that no
	 * input is required.
	 */
	if(!r->foreach && (r->inputs.num_entries > 0 || r->empty_input)) {
		if(do_rule(tf, r, &r->inputs, cwd, clen, NULL, 0) < 0)
			return -1;

		while(!list_empty(&r->inputs.entries)) {
			nle = list_entry(r->inputs.entries.next,
					 struct name_list_entry, list);
			delete_name_list_entry(&r->inputs, nle);
		}
	}

	while(!list_empty(&r->order_only_inputs.entries)) {
		nle = list_entry(r->order_only_inputs.entries.next,
				 struct name_list_entry, list);
		delete_name_list_entry(&r->order_only_inputs, nle);
	}

	return 0;
}

static int input_pattern_to_nl(struct tupfile *tf, char *p,
			       struct name_list *nl, struct bin_list *bl,
			       int lno)
{
	LIST_HEAD(plist);
	LIST_HEAD(symlist);
	struct path_list *pl, *tmp;
	int sym_bork = 0;

	if(get_path_list(p, &plist, tf->tupid, bl, &symlist) < 0)
		return -1;
	while(!list_empty(&symlist)) {
		struct half_entry *he;

		he = list_entry(symlist.next, struct half_entry, list);
		if(he->type == TUP_NODE_GENERATED) {
			fprintf(stderr, "Error: Attempted to input files using a symlink that was generated by tup. Directory %lli, symlink used was %lli, line number %i\n", tf->tupid, he->tupid, lno);
			sym_bork = 1;
		}
		list_del(&he->list);
		free(he);
	}
	if(sym_bork)
		return -1;
	if(parse_dependent_tupfiles(&plist, tf->tupid, tf->g) < 0)
		return -1;
	if(get_name_list(tf, &plist, nl) < 0)
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
		pl->pel = NULL;
		pl->dt = 0;
		pl->bin = NULL;

		if(p[0] == '{') {
			/* Bin */
			char *endb;

			if(!bl) {
				fprintf(stderr, "Parse error: Bins are only usable in an input or output list.\n");
				return -1;
			}

			endb = strchr(p, '}');
			if(!endb) {
				fprintf(stderr, "Parse error: Expecting end bracket for input bin.\n");
				return -1;
			}
			*endb = 0;
			pl->bin = bin_find(p+1, bl);
			if(!pl->bin) {
				fprintf(stderr, "Parse error: Unable to find bin '%s'\n", p+1);
				return -1;
			}
		} else {
			/* Path */
			pl->path = p;
			pl->dt = find_dir_tupid_dt(dt, p, &pl->pel, symlist, 0);
			if(pl->dt <= 0) {
				fprintf(stderr, "Error: Failed to find directory ID for dir '%s' relative to %lli\n", p, dt);
				return -1;
			}
			if(pl->path == pl->pel->path) {
				pl->path = NULL;
			} else {
				/* File points to somewhere later in the path,
				 * so set the last '/' to 0.
				 */
				pl->path[pl->pel->path - pl->path - 1] = 0;
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
		/* Only care about non-bins, and directories that are not our
		 * own.
		 */
		if(!pl->bin && pl->dt != dt) {
			struct node *n;

			n = find_node(g, pl->dt);
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

static int get_name_list(struct tupfile *tf, struct list_head *plist,
			 struct name_list *nl)
{
	struct path_list *pl;

	list_for_each_entry(pl, plist, list) {
		if(pl->bin) {
			if(nl_add_bin(pl->bin, pl->dt, nl) < 0)
				return -1;
		} else {
			if(nl_add_path(tf, pl, nl) < 0)
				return -1;
		}
	}
	return 0;
}

static int char_find(const char *s, int len, const char *list)
{
	int x;
	for(x=0; x<len; x++) {
		const char *p;
		for(p=list; *p; p++) {
			if(s[x] == *p)
				return 1;
		}
	}
	return 0;
}

static int nl_add_path(struct tupfile *tf, struct path_list *pl,
		       struct name_list *nl)
{
	struct build_name_list_args args;

	if(pl->path != NULL) {
		/* Note that dirlen should be pl->pel->path - pl->path - 1,
		 * but we add 1 to account for the trailing '/' that
		 * will be added (since it won't be added in the next case).
		 */
		args.dir = pl->path;
		args.dirlen = pl->pel->path - pl->path;
	} else {
		args.dir = "";
		args.dirlen = 0;
	}
	args.nl = nl;
	args.tf = tf;
	if(char_find(pl->pel->path, pl->pel->len, "*?[") == 0) {
		struct db_node dbn;

		if(tup_db_select_dbn_part(pl->dt, pl->pel->path, pl->pel->len, &dbn) < 0) {
			return -1;
		}
		if(dbn.tupid < 0) {
			fprintf(stderr, "Error: Explicitly named file '%.*s' not found in subdir %lli.\n", pl->pel->len, pl->pel->path, pl->dt);
			tup_db_print(stderr, pl->dt);
			return -1;
		}
		if(dbn.type == TUP_NODE_GHOST) {
			fprintf(stderr, "Error: Explicitly named file '%.*s' is a ghost file, so it can't be used as an input.\n", pl->pel->len, pl->pel->path);
			tup_db_print(stderr, dbn.tupid);
			return -1;
		}
		if(tupid_tree_search(&tf->g->delete_tree, dbn.tupid) != NULL) {
			fprintf(stderr, "Error: Explicitly named file '%.*s' in subdir %lli is scheduled to be deleted (possibly the command that created it has been removed).\n", pl->pel->len, pl->pel->path, pl->dt);
			tup_db_print(stderr, pl->dt);
			return -1;
		}
		if(build_name_list_cb(&args, &dbn) < 0)
			return -1;
	} else {
		if(tup_db_select_node_dir_glob(build_name_list_cb, &args, pl->dt, pl->pel->path, pl->pel->len) < 0)
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

	if(tupid_tree_search(&args->tf->g->delete_tree, dbn->tupid) != NULL)
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
	nle->dirlen = args->dirlen;
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

static int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
		   const char *cwd, int clen, const char *ext, int extlen)
{
	struct name_list onl;
	struct name_list_entry *nle, *onle;
	char *output_pattern;
	char *tcmd;
	char *cmd;
	struct path_list *pl;
	struct tupid_tree *cmd_tt;
	tupid_t cmdid;
	LIST_HEAD(oplist);
	struct rb_root tree = {NULL};

	init_name_list(&onl);

	output_pattern = eval(tf, r->output_pattern, cwd, clen);
	if(!output_pattern)
		return -1;
	if(get_path_list(output_pattern, &oplist, tf->tupid, NULL, NULL) < 0)
		return -1;
	while(!list_empty(&oplist)) {
		pl = list_entry(oplist.next, struct path_list, list);

		if(pl->path) {
			fprintf(stderr, "Error: Attempted to create an output file '%s', which contains a '/' character. Tupfiles should only output files in their own directories.\n - Directory: %lli\n - Rule at line %i: [35m%s[0m\n", pl->path, tf->tupid, r->line_number, r->command);
			return -1;
		}
		onle = malloc(sizeof *onle);
		if(!onle) {
			perror("malloc");
			return -1;
		}
		onle->path = tup_printf(pl->pel->path, pl->pel->len, nl, NULL, NULL, 0);
		if(!onle->path)
			return -1;
		if(strchr(onle->path, '/')) {
			/* Same error as above...uhh, I guess I should rework
			 * this.
			 */
			fprintf(stderr, "Error: Attempted to create an output file '%s', which contains a '/' character. Tupfiles should only output files in their own directories.\n - Directory: %lli\n - Rule at line %i: [35m%s[0m\n", onle->path, tf->tupid, r->line_number, r->command);
			return -1;
		}
		onle->len = strlen(onle->path);
		onle->extlesslen = onle->len - 1;
		while(onle->extlesslen > 0 && onle->path[onle->extlesslen] != '.')
			onle->extlesslen--;

		onle->tupid = tup_db_create_node_part(tf->tupid, onle->path, -1,
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

	tcmd = tup_printf(r->command, -1, nl, &onl, ext, extlen);
	if(!tcmd)
		return -1;
	cmd = eval(tf, tcmd, cwd, clen);
	if(!cmd)
		return -1;
	free(tcmd);

	cmdid = create_command_file(tf->tupid, cmd);
	free(cmd);
	if(cmdid < 0)
		return -1;

	cmd_tt = malloc(sizeof *cmd_tt);
	if(!cmd_tt) {
		perror("malloc");
		return -1;
	}
	cmd_tt->tupid = cmdid;
	if(tupid_tree_insert(&tf->cmd_tree, cmd_tt) < 0) {
		fprintf(stderr, "Error: Attempted to add duplicate command ID %lli\n", cmdid);
		tup_db_print(stderr, cmdid);
		return -1;
	}
	tree_entry_remove(tf->g, cmdid);

	while(!list_empty(&onl.entries)) {
		onle = list_entry(onl.entries.next, struct name_list_entry,
				  list);
		/* Note: If this link becomes sticky, be careful that var/sed
		 * commands still work (since they don't go through the normal
		 * server process to create links).
		 */
		if(tup_db_create_unique_link(cmdid, onle->tupid, &tf->g->delete_tree) < 0) {
			fprintf(stderr, "You may have multiple commands trying to create file '%s'\n", onle->path);
			return -1;
		}
		delete_name_list_entry(&onl, onle);
		tree_entry_remove(tf->g, onle->tupid);
	}

	if(tup_db_write_outputs(cmdid) < 0)
		return -1;
	if(tup_db_clear_tmp_list() < 0)
		return -1;

	list_for_each_entry(nle, &nl->entries, list) {
		if(tupid_tree_add(&tree, nle->tupid, cmdid) < 0)
			return -1;
	}
	list_for_each_entry(nle, &r->order_only_inputs.entries, list) {
		if(tupid_tree_add(&tree, nle->tupid, cmdid) < 0)
			return -1;
	}
	if(tup_db_write_inputs(cmdid, &tree) < 0)
		return -1;
	return 0;
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

static const char *find_char(const char *s, int len, char c)
{
	int x;
	for(x=0; x<len; x++) {
		if(s[x] == c)
			return &s[x];
	}
	return NULL;
}

static char *tup_printf(const char *cmd, int cmd_len, struct name_list *nl,
			struct name_list *onl, const char *ext, int extlen)
{
	struct name_list_entry *nle;
	char *s;
	int x;
	const char *p;
	const char *next;
	const char *spc;
	int clen = strlen(cmd);

	if(cmd_len == -1) {
		cmd_len = strlen(cmd);
	}
	clen = cmd_len;

	p = cmd;
	while((p = find_char(p, cmd+cmd_len - p, '%')) !=  NULL) {
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
		} else if(*p == 'e') {
			if(!ext) {
				fprintf(stderr, "Error: %%e is only valid with a foreach rule for files that have extensions.\n");
				return NULL;
			}
			clen += extlen;
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
	while((next = find_char(p, cmd+cmd_len - p, '%')) !=  NULL) {
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
		} else if(*next == 'e') {
			memcpy(&s[x], ext, extlen);
			x += extlen;
			memcpy(&s[x], p, spc - p);
			x += spc - p;
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
	memcpy(&s[x], p, cmd+cmd_len - p + 1);
	if((signed)strlen(s) != clen) {
		fprintf(stderr, "Error: Calculated string length (%i) didn't match actual (%li). String is: '%s'.\n", clen, (long)strlen(s), s);
		return NULL;
	}
	return s;
}

static char *eval(struct tupfile *tf, const char *string,
		  const char *cwd, int clen)
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
			if((s[1] == '$' || s[1] == '@') && s[2] == '(') {
				/* \$( becomes $( */
				/* \@( becomes @ */
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
				if(rparen-var == 7 &&
				   strncmp(var, "TUP_CWD", 7) == 0) {
					if(!cwd) {
						fprintf(stderr, "Error: TUP_CWD is only valid in variable and rule definitions.\n");
						return NULL;
					}
					len += clen;
				} else {
					vlen = vardb_len(&tf->vdb, var, rparen-var);
					if(vlen < 0)
						return NULL;
					len += vlen;
				}
				s = rparen + 1;
			} else {
				s++;
				len++;
			}
		} else if(*s == '@') {
			const char *rparen;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					expected = "ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				vlen = tup_db_get_varlen(var, rparen-var);
				if(vlen < 0)
					return NULL;
				len += vlen;
				s = rparen + 1;
			} else {
				s++;
				len++;
			}
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
			if((s[1] == '$' || s[1] == '@') && s[2] == '(') {
				/* \$( becomes $( */
				/* \@( becomes @ */
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
				if(rparen-var == 7 &&
				   strncmp(var, "TUP_CWD", 7) == 0) {
					if(!cwd) {
						fprintf(stderr, "Error: TUP_CWD is only valid in variable definitions.\n");
						return NULL;
					}
					memcpy(p, cwd, clen);
					p += clen;
				} else {
					if(vardb_copy(&tf->vdb, var, rparen-var, &p) < 0)
						return NULL;
				}
				s = rparen + 1;
			} else {
				*p = *s;
				p++;
				s++;
			}
		} else if(*s == '@') {
			const char *rparen;
			tupid_t vt;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					expected = "ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				vt = tup_db_get_var(var, rparen-var, &p);
				if(vt < 0)
					return NULL;
				if(tup_db_create_link(vt, tf->tupid, TUP_LINK_NORMAL) < 0)
					return NULL;
				s = rparen + 1;
			} else {
				*p = *s;
				p++;
				s++;
			}
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
