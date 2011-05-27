/* _ATFILE_SOURCE needed at least on linux x86_64 */
#define _ATFILE_SOURCE
#include "parser.h"
#include "linux/list.h"
#include "flist.h"
#include "fileio.h"
#include "pel_group.h"
#include "fslurp.h"
#include "db.h"
#include "vardb.h"
#include "graph.h"
#include "config.h"
#include "bin.h"
#include "entry.h"
#include "string_tree.h"
#include "compat.h"
#include "if_stmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#define SYNTAX_ERROR -2

struct name_list {
	struct list_head entries;
	int num_entries;
	int totlen;
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
	struct tup_entry *tent;
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
	int foreach;
	char *input_pattern;
	char *output_pattern;
	struct bin *bin;
	char *command;
	int command_len;
	struct name_list inputs;
	struct name_list order_only_inputs;
	struct name_list bang_oo_inputs;
	char *bang_extra_outputs;
	int empty_input;
	int line_number;
	struct name_list *output_nl;
};

struct bang_rule {
	struct string_tree st;
	int foreach;
	char *value;
	char *input;
	char *command;
	int command_len;
	char *output_pattern;
	char *extra_outputs;
};

struct bang_list {
	struct list_head list;
	struct bang_rule *br;
};

struct src_chain {
	struct list_head list;
	char *input_pattern;
	struct list_head banglist;
};

struct chain {
	struct string_tree st;
	struct list_head src_chain_list;
	struct list_head banglist;
};

struct build_name_list_args {
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
	struct rb_root bang_tree;
	struct rb_root input_tree;
	struct rb_root chain_tree;
	int ign;
};

static int parse_tupfile(struct tupfile *tf, struct buf *b, tupid_t curdir,
			 const char *cwd, int clen);
static int var_ifdef(struct tupfile *tf, const char *var);
static int eval_eq(struct tupfile *tf, char *expr, char *eol);
static int include_rules(struct tupfile *tf, tupid_t curdir,
			 const char *cwd, int clen);
static int gitignore(struct tupfile *tf);
static int include_name_list(struct tupfile *tf, struct name_list *nl,
			     const char *cwd, int clen);
static int parse_rule(struct tupfile *tf, char *p, int lno, struct bin_list *bl,
		      const char *cwd, int clen);
static int parse_bang_definition(struct tupfile *tf, char *p, int lno);
static int parse_chain_definition(struct tupfile *tf, char *p, int lno);
static int parse_empty_bang_rule(struct tupfile *tf, struct rule *r,
				 const char *cwd, int clen);
static int parse_bang_rule(struct tupfile *tf, struct rule *r,
			   struct name_list *nl,const char *ext,
			   const char *cwd, int clen);
static void free_bang_tree(struct rb_root *root);
static void free_chain_tree(struct rb_root *root);
static void free_banglist(struct list_head *list);
static int split_input_pattern(char *p, char **o_input, char **o_cmd,
			       int *o_cmdlen, char **o_output, char **o_bin,
			       int *swapio);
static int parse_input_pattern(struct tupfile *tf, char *input_pattern,
			       struct name_list *inputs,
			       struct name_list *order_only_inputs,
			       struct bin_list *bl, int lno,
			       const char *cwd, int clen, int required);
static int execute_rule(struct tupfile *tf, struct rule *r, struct bin_list *bl,
			const char *cwd, int clen);
static int __execute_rule(struct tupfile *tf, struct rule *r,
			  struct name_list *output_nl, const char *cwd, int clen);
static int execute_reverse_rule(struct tupfile *tf, struct rule *r,
				struct bin_list *bl, const char *cwd, int clen);
static int check_recursive_chain(struct tupfile *tf, const char *input_pattern,
				 struct bin_list *bl,
				 struct rule *r, const char *ext,
				 const char *cwd, int clen);
static int input_pattern_to_nl(struct tupfile *tf, char *p,
			       struct name_list *nl, struct bin_list *bl,
			       int lno, int required);
static int get_path_list(char *p, struct list_head *plist, tupid_t dt,
			 struct bin_list *bl, struct rb_root *symtree);
static void make_path_list_unique(struct list_head *plist);
static void del_pl(struct path_list *pl);
static void make_name_list_unique(struct name_list *nl);
static int parse_dependent_tupfiles(struct list_head *plist, struct tupfile *tf,
				    struct graph *g);
static int get_name_list(struct tupfile *tf, struct list_head *plist,
			 struct name_list *nl, int required);
static int nl_add_path(struct tupfile *tf, struct path_list *pl,
		       struct name_list *nl, int required);
static int nl_add_bin(struct bin *b, struct name_list *nl);
static int build_name_list_cb(void *arg, struct tup_entry *tent);
static char *set_path(const char *name, const char *dir, int dirlen);
static int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
		   const char *cwd, int clen, const char *ext, int extlen,
		   struct name_list *output_nl);
static void init_name_list(struct name_list *nl);
static void set_nle_base(struct name_list_entry *nle);
static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle);
static void delete_name_list(struct name_list *nl);
static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle);
static void move_name_list_entry(struct name_list *newnl, struct name_list *oldnl,
				 struct name_list_entry *nle);
static void move_name_list(struct name_list *newnl, struct name_list *oldnl);
static char *tup_printf(const char *cmd, int cmd_len, struct name_list *nl,
			struct name_list *onl, const char *ext, int extlen,
			int is_command);
static char *eval(struct tupfile *tf, const char *string,
		  const char *cwd, int clen);

int parse(struct node *n, struct graph *g)
{
	struct tupfile tf;
	int fd;
	int rc = -1;
	struct buf b;

	if(n->parsing) {
		fprintf(stderr, "Error: Circular dependency found among Tupfiles (last dir ID %lli  = '%s').\nThis is madness!\n", n->tnode.tupid, n->tent->name.s);
		return -1;
	}
	n->parsing = 1;

	tf.tupid = n->tnode.tupid;
	tf.g = g;
	tf.cmd_tree.rb_node = NULL;
	tf.bang_tree.rb_node = NULL;
	tf.input_tree.rb_node = NULL;
	tf.chain_tree.rb_node = NULL;
	tf.ign = 0;
	if(vardb_init(&tf.vdb) < 0)
		return -1;

	/* Keep track of the commands and generated files that we had created
	 * previously. We'll check these against the new ones in order to see
	 * if any should be removed.
	 */
	if(tup_db_dirtype_to_tree(tf.tupid, &g->delete_tree, &g->delete_count, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_dirtype_to_tree(tf.tupid, &g->delete_tree, &g->delete_count, TUP_NODE_GENERATED) < 0)
		return -1;

	tf.dfd = tup_entry_open(n->tent);
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
	if(tup_db_write_dir_inputs(tf.tupid, &tf.input_tree) < 0)
		return -1;
out_close_file:
	close(fd);
out_close_dfd:
	close(tf.dfd);
out_close_vdb:
	if(vardb_close(&tf.vdb) < 0)
		rc = -1;
	free_chain_tree(&tf.chain_tree);
	free_tupid_tree(&tf.cmd_tree);
	free_bang_tree(&tf.bang_tree);
	free_tupid_tree(&tf.input_tree);

	return rc;
}

static int parse_tupfile(struct tupfile *tf, struct buf *b, tupid_t curdir,
			 const char *cwd, int clen)
{
	char *p, *e;
	char *line;
	int lno = 0;
	struct bin_list bl;
	struct if_stmt ifs;

	if(bin_list_init(&bl) < 0)
		return -1;
	if_init(&ifs);

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
		while(newline[-1] == '\\' || (newline[-2] == '\\' && newline[-1] == '\r')) {
			if (newline[-1] == '\r') {
				newline[-2] = ' ';
			}
			newline[-1] = ' ';
			newline[0] = ' ';
			newline = strchr(p, '\n');
			if(!newline)
				goto syntax_error;
			lno++;
		}

		p = newline + 1;

		/* Remove trailing whitespace */
		while(newline > line && isspace(newline[-1])) {
			newline--;
		}
		*newline = 0;

		if(line[0] == '#') {
			/* Skip comments */
			continue;
		}

		if(strcmp(line, "else") == 0) {
			if(if_else(&ifs) < 0)
				goto syntax_error;
		} else if(strcmp(line, "endif") == 0) {
			if(if_endif(&ifs) < 0)
				goto syntax_error;
		} else if(strncmp(line, "ifeq ", 5) == 0) {
			int rc = 0;
			if(if_true(&ifs)) {
				rc = eval_eq(tf, line+5, newline);
				if(rc == SYNTAX_ERROR)
					goto syntax_error;
				if(rc < 0)
					return -1;
			}
			if(if_add(&ifs, rc) < 0)
				goto syntax_error;
		} else if(strncmp(line, "ifneq ", 6) == 0) {
			int rc = 0;
			if(if_true(&ifs)) {
				rc = eval_eq(tf, line+6, newline);
				if(rc == SYNTAX_ERROR)
					goto syntax_error;
				if(rc < 0)
					return -1;
			}
			if(if_add(&ifs, !rc) < 0)
				goto syntax_error;
		} else if(strncmp(line, "ifdef ", 6) == 0) {
			int rc = 0;
			if(if_true(&ifs)) {
				rc = var_ifdef(tf, line+6);
				if(rc < 0)
					return -1;
			}
			if(if_add(&ifs, rc) < 0)
				goto syntax_error;
		} else if(strncmp(line, "ifndef ", 7) == 0) {
			int rc = 0;
			if(if_true(&ifs)) {
				rc = var_ifdef(tf, line+7);
				if(rc < 0)
					return -1;
			}
			if(if_add(&ifs, !rc) < 0)
				goto syntax_error;
		} else if(!if_true(&ifs)) {
			/* Skip the false part of an if block */
		} else if(strncmp(line, "include ", 8) == 0) {
			char *file;
			struct name_list nl;
			struct path_list *pl, *tmppl;
			int sym_bork = 0;
			LIST_HEAD(plist);
			struct rb_root symtree = RB_ROOT;
			struct rb_node *rbn;
			struct tup_entry *tent;
			tupid_t symid;

			init_name_list(&nl);

			file = line + 8;
			file = eval(tf, file, NULL, 0);
			if(!file)
				return -1;
			if(get_path_list(file, &plist, curdir, NULL, &symtree) < 0)
				return -1;
			tupid_tree_for_each(symid, rbn, &symtree) {
				tent = tup_entry_get(symid);
				if(tent->type == TUP_NODE_GENERATED) {
					fprintf(stderr, "Error: Attempted to include a Tupfile from a path that contains a symlink generated by tup. Directory %lli, symlink used was %lli, line number %i\n", tf->tupid, tent->tnode.tupid, lno);
					sym_bork = 1;
				}
			}
			if(sym_bork)
				return -1;
			free_tupid_tree(&symtree);

			if(get_name_list(tf, &plist, &nl, 1) < 0)
				return -1;
			list_for_each_entry_safe(pl, tmppl, &plist, list) {
				del_pl(pl);
			}
			/* Can only be freed after plist */
			free(file);
			if(include_name_list(tf, &nl, cwd, clen) < 0)
				return -1;
		} else if(strcmp(line, "include_rules") == 0) {
			if(include_rules(tf, curdir, cwd, clen) < 0)
				return -1;
		} else if(strcmp(line, ".gitignore") == 0) {
			tf->ign = 1;
		} else if(line[0] == ':') {
			if(parse_rule(tf, line+1, lno, &bl, cwd, clen) < 0)
				goto syntax_error;
		} else if(line[0] == '!') {
			if(parse_bang_definition(tf, line, lno) < 0)
				goto syntax_error;
		} else if(line[0] == '*') {
			if(parse_chain_definition(tf, line, lno) < 0)
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

			if(strncmp(var, "CONFIG_", 7) == 0) {
				fprintf(stderr, "Error: Unable to override setting of variable '%s' because it begins with 'CONFIG_'. These variables can only be set in the tup.config file.\n", var);
				return -1;
			}

			if(append)
				rc = vardb_append(&tf->vdb, var, value);
			else
				rc = vardb_set(&tf->vdb, var, value, NULL);
			if(rc < 0) {
				fprintf(stderr, "Error setting variable '%s'\n", var);
				return -1;
			}
			free(var);
			free(value);
		}
	}
	if(if_check(&ifs) < 0) {
		fprintf(stderr, "Error parsing Tupfile [%lli]: missing endif before EOF.\n", tf->tupid);
		tup_db_print(stderr, tf->tupid);
		return -1;
	}

	bin_list_del(&bl);

	return 0;

syntax_error:
	fprintf(stderr, "Error parsing Tupfile [%lli] line %i\n  Line was: '%s'\n", tf->tupid, lno, line);
	tup_db_print(stderr, tf->tupid);
	return -1;
}

static int eval_eq(struct tupfile *tf, char *expr, char *eol)
{
	char *paren;
	char *comma;
	char *lval;
	char *rval;
	int rc;

	paren = strchr(expr, '(');
	if(!paren)
		return SYNTAX_ERROR;
	lval = paren + 1;
	comma = strchr(lval, ',');
	if(!comma)
		return SYNTAX_ERROR;
	rval = comma + 1;

	paren = eol;
	while(paren > expr) {
		if(*paren == ')')
            goto found_paren;
		paren--;
	}
	return SYNTAX_ERROR;
found_paren:
	*comma = 0;
	*paren = 0;

	lval = eval(tf, lval, NULL, 0);
	if(!lval)
		return -1;
	rval = eval(tf, rval, NULL, 0);
	if(!rval) {
		free(lval);
		return -1;
	}

	if(strcmp(lval, rval) == 0)
		rc = 1;
	else
		rc = 0;
	free(lval);
	free(rval);

	return rc;
}

static int var_ifdef(struct tupfile *tf, const char *var)
{
	struct tup_entry *tent;
	int rc;

	if(strncmp(var, "CONFIG_", 7) == 0)
		var += 7;
	tent = tup_db_get_var(var, -1, NULL);
	if(!tent)
		return -1;
	if(tent->type == TUP_NODE_VAR) {
		rc = 1;
	} else {
		rc = 0;
	}
	if(tupid_tree_add_dup(&tf->input_tree, tent->tnode.tupid) < 0)
		return -1;
	return rc;
}

static int include_rules(struct tupfile *tf, tupid_t curdir,
			 const char *cwd, int clen)
{
	struct tup_entry *tent;
	int num_dotdots;
	int x;
	char *p;
	char *path;
	int plen;
	char tuprules[] = "Tuprules.tup";
	int trlen = sizeof(tuprules) - 1;
	struct name_list nl;
	struct build_name_list_args args;

	num_dotdots = 0;
	tent = tup_entry_get(curdir);
	if(!tent)
		return -1;
	while(tent->tnode.tupid != DOT_DT) {
		tent = tent->parent;
		num_dotdots++;
	}
	path = malloc(num_dotdots * 3 + trlen + 1);
	if(!path) {
		perror("malloc");
		return -1;
	}
	p = path;
	for(x=0; x<num_dotdots; x++) {
		strcpy(p, ".." PATH_SEP_STR);
		p += 3;
	}
	strcpy(path + num_dotdots*3, tuprules);
	/* Plen only includes the length of the path */
	plen = num_dotdots*3;

	init_name_list(&nl);
	args.nl = &nl;

	p = path;
	for(x=0; x<=num_dotdots; x++, p+=3, plen-=3) {
		if(gimme_node_or_make_ghost(curdir, p, &tent) < 0)
			return -1;
		if(tent->type == TUP_NODE_GHOST) {
			if(tupid_tree_add_dup(&tf->input_tree, tent->tnode.tupid) < 0)
				return -1;
			continue;
		}

		args.dir = p;
		args.dirlen = plen;
		if(build_name_list_cb(&args, tent) < 0)
			return -1;
	}
	free(path);
	return include_name_list(tf, &nl, cwd, clen);
}

static int gitignore(struct tupfile *tf)
{
	char *s;
	int len;
	int fd;

	if(tup_db_alloc_generated_nodelist(&s, &len, tf->tupid, &tf->g->delete_tree) < 0)
		return -1;
	if((s && len) || tf->tupid == 1) {
		struct tup_entry *tent;

		if(tup_db_select_tent(tf->tupid, ".gitignore", &tent) < 0)
			return -1;
		if(!tent) {
			if(tup_db_node_insert(tf->tupid, ".gitignore", -1, TUP_NODE_GENERATED, -1) == NULL)
				return -1;
		} else {
			tree_entry_remove(&tf->g->delete_tree,
					  tent->tnode.tupid,
					  &tf->g->delete_count);
		}

		fd = openat(tf->dfd, ".gitignore", O_CREAT|O_WRONLY|O_TRUNC, 0666);
		if(fd < 0) {
			perror(".gitignore");
			return -1;
		}
		if(tf->tupid == 1) {
			if(write(fd, ".tup\n", 5) < 0) {
				perror("write");
				goto err_close;
			}
		}
		if(write(fd, "/.*.swp\n", 8) < 0) {
			perror("write");
			goto err_close;
		}
		if(write(fd, "/.gitignore\n", 12) < 0) {
			perror("write");
			goto err_close;
		}
		if(s && len) {
			if(write(fd, s, len) < 0) {
				perror("write");
				goto err_close;
			}
		}
		close(fd);
	}
	if(s) {
		free(s); /* Freeze gopher! */
	}
	return 0;

err_close:
	close(fd);
	return -1;
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

		if(nle->tent->type != TUP_NODE_FILE) {
			if(nle->tent->type == TUP_NODE_GENERATED) {
				fprintf(stderr, "Error: Unable to include generated file '%s'. Your build configuration must be comprised of files you wrote yourself.\n", nle->path);
				return -1;
			} else {
				fprintf(stderr, "tup error: Attempt to include node (ID %lli, name='%s') of type %i?\n", nle->tent->tnode.tupid, nle->path, nle->tent->type);
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
				newcwd[clen] = PATH_SEP;
				memcpy(newcwd+clen+1, nle->path, nle->dirlen-1);
				newcwd[newclen] = 0;
			}
			cnc = newcwd;
		} else {
			cnc = cwd;
			newclen = clen;
		}

		fd = tup_entry_open(nle->tent);
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

		/* When parsing the included Tupfile, any files it includes
		 * will be relative to it (nle->tent->dt), not to the parent
		 * dir (tupid).  However, we want all links to be made to the
		 * parent tupid.
		 */
		rc = parse_tupfile(tf, &incb, nle->tent->dt, cnc, newclen);
		free(incb.s);
		if(newcwd)
			free(newcwd);
		if(rc < 0) {
			fprintf(stderr, "Error parsing included file '%s'\n", nle->path);
			return -1;
		}

		if(tupid_tree_add_dup(&tf->input_tree, nle->tent->tnode.tupid) < 0)
			return -1;
		delete_name_list_entry(nl, nle);
	}
	return 0;
}

static int parse_rule(struct tupfile *tf, char *p, int lno, struct bin_list *bl,
		      const char *cwd, int clen)
{
	char *input, *cmd, *output, *bin;
	int cmd_len;
	struct rule r;
	int rc;
	int swapio;

	if(split_input_pattern(p, &input, &cmd, &cmd_len, &output, &bin, &swapio) < 0)
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
	r.input_pattern = input;
	r.output_pattern = output;
	r.command = cmd;
	r.command_len = cmd_len;
	r.line_number = lno;
	r.output_nl = NULL;

	if(swapio)
		rc = execute_reverse_rule(tf, &r, bl, cwd, clen);
	else
		rc = execute_rule(tf, &r, bl, cwd, clen);
	return rc;
}

static char *split_eq(char *p)
{
	char *eq;
	char *value;

	eq = strchr(p, '=');
	if(!eq) {
		return NULL;
	}
	value = eq + 1;
	while(*value && isspace(*value))
		value++;
	eq--;
	while(isspace(*eq))
		eq--;
	eq[1] = 0;
	return value;
}

static struct bang_rule *alloc_br(void)
{
	struct bang_rule *br;
	br = malloc(sizeof *br);
	if(!br) {
		perror("malloc");
		return NULL;
	}
	br->value = NULL;
	br->input = NULL;
	br->command = NULL;
	br->output_pattern = NULL;
	br->extra_outputs = NULL;
	return br;
}

static int set_br_extra_outputs(struct bang_rule *br)
{
	char *sep;

	sep = strchr(br->output_pattern, '|');
	if(sep != NULL) {
		*sep = 0;
		br->extra_outputs = strdup(sep + 1);
		if(!br->extra_outputs) {
			perror("strdup");
			return -1;
		}
	} else {
		br->extra_outputs = NULL;
	}
	return 0;
}

static int parse_bang_definition(struct tupfile *tf, char *p, int lno)
{
	struct string_tree *st;
	char *input;
	char *command;
	int command_len;
	int foreach = 0;
	char *output;
	char *value;
	char *alloc_value;
	char *bin;
	int swapio = 0;
	struct bang_rule *br;

	value = split_eq(p);
	if(!value) {
		fprintf(stderr, "Parse error line %i: Expecting '=' to set the bang rule.\n", lno);
		return -1;
	}

	if(value[0] == '!') {
		/* Alias one macro as another */
		struct bang_rule *cur_br;

		st = string_tree_search(&tf->bang_tree, value, -1);
		if(!st) {
			fprintf(stderr, "Error: Unable to find !-macro '%s'\n", value);
			return -1;
		}
		cur_br = container_of(st, struct bang_rule, st);

		br = alloc_br();
		if(!br) {
			return -1;
		}
		br->foreach = cur_br->foreach;
		br->value = NULL;
		if(cur_br->input) {
			br->input = strdup(cur_br->input);
			if(!br->input) {
				perror("strdup");
				goto err_cleanup_br;
			}
		} else {
			br->input = NULL;
		}
		br->command = strdup(cur_br->command);
		if(!br->command) {
			perror("strdup");
			goto err_cleanup_br;
		}
		br->output_pattern = strdup(cur_br->output_pattern);
		if(!br->output_pattern) {
			perror("strdup");
			goto err_cleanup_br;
		}
		if(cur_br->extra_outputs) {
			br->extra_outputs = strdup(cur_br->extra_outputs);
			if(!br->extra_outputs) {
				perror("strdup");
				goto err_cleanup_br;
			}
		} else {
			br->extra_outputs = NULL;
		}

		br->command_len = cur_br->command_len;

		if(string_tree_add(&tf->bang_tree, &br->st, p) < 0) {
			fprintf(stderr, "Error inserting bang rule into tree\n");
			goto err_cleanup_br;
		}
		return 0;
	}

	alloc_value = strdup(value);
	if(!alloc_value) {
		perror("strdup");
		return -1;
	}

	if(split_input_pattern(alloc_value, &input, &command, &command_len, &output,
			       &bin, &swapio) < 0)
		return -1;
	if(bin != NULL) {
		fprintf(stderr, "Error: bins aren't allowed in !-macros. Rule was: %s = %s\n", p, alloc_value);
		return -1;
	}
	if(swapio) {
		fprintf(stderr, "Error: !-macro must use '|>' separators\n");
		return -1;
	}

	if(input) {
		if(strncmp(input, "foreach", 7) == 0) {
			foreach = 1;
			input += 7;
			while(*input == ' ') input++;
		}
	}

	st = string_tree_search(&tf->bang_tree, p, -1);
	if(st) {
		/* Replace existing !-macro */
		struct bang_rule *cur_br;

		cur_br = container_of(st, struct bang_rule, st);
		free(cur_br->value);
		cur_br->foreach = foreach;
		cur_br->value = alloc_value;
		cur_br->input = input;
		cur_br->command = command;
		cur_br->command_len = command_len;
		cur_br->output_pattern = output;
		free(cur_br->extra_outputs);
		if(set_br_extra_outputs(cur_br) < 0)
			return -1;
	} else {
		/* Create new !-macro */
		br = alloc_br();
		if(!br) {
			return -1;
		}
		br->foreach = foreach;
		br->input = input;
		br->command = command;
		br->command_len = command_len;
		br->output_pattern = output;
		if(set_br_extra_outputs(br) < 0)
			goto err_cleanup_br;
		br->value = alloc_value;

		if(string_tree_add(&tf->bang_tree, &br->st, p) < 0) {
			fprintf(stderr, "Error inserting bang rule into tree\n");
			goto err_cleanup_br;
		}
	}
	return 0;

err_cleanup_br:
	free(br->extra_outputs);
	free(br->output_pattern);
	free(br->command);
	free(br->input);
	free(br->value);
	free(br);
	return -1;
}

static int parse_chain_definition(struct tupfile *tf, char *p, int lno)
{
	struct string_tree *st;
	char *value;
	struct chain *ch;
	char *lbracket;
	char *input_pattern = NULL;
	struct list_head *destlist;

	value = split_eq(p);
	if(!value) {
		fprintf(stderr, "Parse error line %i: Expecting '=' to set the *-chain.\n", lno);
		return -1;
	}

	lbracket = strchr(p, '[');
	if(lbracket) {
		char *rbracket;
		*lbracket = 0;
		input_pattern = lbracket+1;
		rbracket = strchr(input_pattern, ']');
		if(!rbracket) {
			fprintf(stderr, "Parse error line %i: Expecting ']' character in *-chain definition\n", lno);
			return -1;
		}
		*rbracket = 0;
	}
	st = string_tree_search(&tf->chain_tree, p, -1);
	if(st) {
		/* Replace existing *-chain */
		ch = container_of(st, struct chain, st);
		if(!input_pattern) {
			free_banglist(&ch->banglist);
		}
	} else {
		/* Create new *-chain */
		ch = malloc(sizeof *ch);
		if(!ch) {
			perror("malloc");
			return -1;
		}
		INIT_LIST_HEAD(&ch->src_chain_list);
		INIT_LIST_HEAD(&ch->banglist);

		if(string_tree_add(&tf->chain_tree, &ch->st, p) < 0) {
			fprintf(stderr, "Error inserting *-chain into tree\n");
			return -1;
		}
	}

	destlist = &ch->banglist;
	if(input_pattern) {
		struct src_chain *sc;
		sc = malloc(sizeof *sc);
		if(!sc) {
			perror("malloc");
			return -1;
		}
		sc->input_pattern = strdup(input_pattern);
		if(!sc->input_pattern) {
			perror("strdup");
			free(sc);
			return -1;
		}
		INIT_LIST_HEAD(&sc->banglist);
		list_add_tail(&sc->list, &ch->src_chain_list);
		destlist = &sc->banglist;
	}

	do {
		struct string_tree *bst;
		struct bang_rule *br;
		struct bang_list *bal;
		char *ce;
		p = strstr(value, "|>");
		if(p) {
			ce = p-1;
			while(isspace(*ce) && ce > value)
				ce--;
			ce[1] = 0;
			p += 2;
			while(*p && isspace(*p))
				p++;
		}
		if(value[0] != '!') {
			fprintf(stderr, "Error: *-chain must be composed of !-macros, not '%s'\n", value);
			return -1;
		}
		bst = string_tree_search(&tf->bang_tree, value, -1);
		if(!bst) {
			fprintf(stderr, "Error: Unable to find !-macro: '%s'\n", value);
			return -1;
		}
		br = container_of(bst, struct bang_rule, st);

		bal = malloc(sizeof *bal);
		if(!bal) {
			perror("malloc");
			return -1;
		}
		bal->br = br;
		list_add_tail(&bal->list, destlist);

		value = p;
	} while(p != NULL);

	return 0;
}

static int __parse_bang_rule(struct tupfile *tf, struct rule *r,
			     struct string_tree *st, struct name_list *nl,
			     const char *cwd, int clen)
{
	struct bang_rule *br;
	char *tinput;
	br = container_of(st, struct bang_rule, st);

	/* Add any order only inputs to the list */
	if(nl && br->input) {
		tinput = tup_printf(br->input, -1, nl, NULL, NULL, 0, 0);
		if(!tinput)
			return -1;
	} else {
		tinput = br->input;
	}
	if(parse_input_pattern(tf, tinput, NULL, &r->bang_oo_inputs, NULL,
			       r->line_number, cwd, clen, 1) < 0)
		return -1;
	if(nl) {
		free(tinput);
	}

	/* The command gets replaced whole-sale */
	r->command = br->command;
	r->command_len = br->command_len;

	/* If the rule didn't specify any output pattern, use the one from the
	 * !-macro.
	 */
	if(!r->output_pattern[0])
		r->output_pattern = br->output_pattern;

	/* Also include any extra outputs from the !-macro. These may specify
	 * additional outputs that the user of the !-macro doesn't know about
	 * (such as command side-effects).
	 */
	r->bang_extra_outputs = br->extra_outputs;
	return 0;
}

static int parse_empty_bang_rule(struct tupfile *tf, struct rule *r,
				 const char *cwd, int clen)
{
	struct string_tree *st;

	st = string_tree_search2(&tf->bang_tree, r->command, r->command_len,
				 ".EMPTY");
	if(!st)
		return 1;
	return __parse_bang_rule(tf, r, st, NULL, cwd, clen);
}

static int parse_bang_rule(struct tupfile *tf, struct rule *r,
			   struct name_list *nl, const char *ext,
			   const char *cwd, int clen)
{
	struct string_tree *st;

	/* First try to find the extension-specific rule, and if not then use
	 * the general one. Eg: if the input is foo.c, then the extension is ".c",
	 * so try "!cc.c" first, then "!cc" second.
	 */
	st = string_tree_search2(&tf->bang_tree, r->command, r->command_len, ext);
	if(!st) {
		st = string_tree_search2(&tf->bang_tree, r->command, r->command_len, NULL);
		if(!st) {
			fprintf(stderr, "Error finding bang variable: '%s'\n",
				r->command);
			return -1;
		}
	}
	return __parse_bang_rule(tf, r, st, nl, cwd, clen);
}

static void free_bang_tree(struct rb_root *root)
{
	struct rb_node *rbn;

	while((rbn = rb_first(root)) != NULL) {
		struct string_tree *st = rb_entry(rbn, struct string_tree, rbn);
		struct bang_rule *br = container_of(st, struct bang_rule, st);

		string_tree_free(root, st);

		if(br->value) {
			/* For regular macros */
			free(br->value);
		} else {
			/* For aliased macros */
			free(br->input);
			free(br->command);
			free(br->output_pattern);
		}
		free(br->extra_outputs);
		free(br);
	}
}

static void free_chain_tree(struct rb_root *root)
{
	struct rb_node *rbn;

	while((rbn = rb_first(root)) != NULL) {
		struct string_tree *st = rb_entry(rbn, struct string_tree, rbn);
		struct chain *ch = container_of(st, struct chain, st);
		struct src_chain *sc;

		string_tree_free(root, st);

		while(!list_empty(&ch->src_chain_list)) {
			sc = list_entry(ch->src_chain_list.next, struct src_chain, list);
			list_del(&sc->list);
			free_banglist(&sc->banglist);
			free(sc->input_pattern);
			free(sc);
		}

		free_banglist(&ch->banglist);
		free(ch);
	}
}

static void free_banglist(struct list_head *list)
{
	struct bang_list *bal;
	while(!list_empty(list)) {
		bal = list_entry(list->next, struct bang_list, list);
		list_del(&bal->list);
		free(bal);
	}
}

static int split_input_pattern(char *p, char **o_input, char **o_cmd,
			       int *o_cmdlen, char **o_output, char **o_bin,
			       int *swapio)
{
	char *input;
	char *cmd;
	char *output;
	char *bin = NULL;
	char *brace;
	char *ie, *ce, *oe;
	char *tmp;
	const char *marker = "|>";

	input = p;
	while(isspace(*input))
		input++;
	tmp = strstr(p, marker);
	if(tmp) {
		*swapio = 0;
		p = tmp;
	} else {
		*swapio = 1;
		marker = "<|";
		p = strstr(p, marker);
		if(!p)
			return -1;
	}
	if(input < p) {
		ie = p - 1;
		while(isspace(*ie) && ie > input)
			ie--;
		ie[1] = 0;
	} else {
		input = NULL;
	}
	p += 2;
	cmd = p;
	while(isspace(*cmd))
		cmd++;

	p = strstr(p, marker);
	if(!p)
		return -1;
	ce = p - 1;
	while(isspace(*ce) && ce > cmd)
		ce--;
	p += 2;
	output = p;
	while(isspace(*output))
		output++;
	ce[1] = 0;

	brace = strchr(output, '{');
	if(brace) {
		oe = brace - 1;
		while(isspace(*oe) && oe > output)
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
	*o_cmdlen = ce - cmd + 1;
	*o_output = output;
	*o_bin = bin;
	return 0;
}

static int parse_input_pattern(struct tupfile *tf, char *input_pattern,
			       struct name_list *inputs,
			       struct name_list *order_only_inputs,
			       struct bin_list *bl, int lno,
			       const char *cwd, int clen, int required)
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
		if(input_pattern_to_nl(tf, oosep, order_only_inputs, bl, lno, required) < 0)
			return -1;
	}
	if(inputs) {
		if(input_pattern_to_nl(tf, eval_pattern, inputs, bl, lno, required) < 0)
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

static int execute_rule(struct tupfile *tf, struct rule *r, struct bin_list *bl,
			const char *cwd, int clen)
{
	struct name_list output_nl;
	struct name_list_entry *nle;
	char *last_output_pattern;
	char empty_pattern[] = "";

	init_name_list(&r->inputs);
	init_name_list(&r->order_only_inputs);
	init_name_list(&r->bang_oo_inputs);
	r->bang_extra_outputs = NULL;
	if(parse_input_pattern(tf, r->input_pattern, &r->inputs,
			       &r->order_only_inputs, bl, r->line_number,
			       cwd, clen, 1) < 0)
		return -1;

	make_name_list_unique(&r->inputs);

	init_name_list(&output_nl);

	if(r->command[0] == '*') {
		struct string_tree *st;
		struct chain *ch;
		struct bang_list *bal;
		struct list_head *banglist;

		last_output_pattern = r->output_pattern;
		r->output_pattern = empty_pattern;

		st = string_tree_search(&tf->chain_tree, r->command, r->command_len);
		if(!st) {
			fprintf(stderr, "Error: Unable to find *-chain: '%s'\n", r->command);
			return -1;
		}
		ch = container_of(st, struct chain, st);

		banglist = &ch->banglist;
		if(!r->inputs.num_entries && r->output_nl) {
			struct src_chain *sc;
			const char *ext = NULL;

			if(r->output_nl->num_entries > 0) {
				nle = list_entry(r->output_nl->entries.next, struct name_list_entry, list);
				if(nle->base &&
				   nle->extlessbaselen != nle->baselen) {
					ext = nle->base + nle->extlessbaselen;
				}
			}
			list_for_each_entry(sc, &ch->src_chain_list, list) {
				char *tinput;
				char *input_pattern;

				banglist = &sc->banglist;

				tinput = tup_printf(sc->input_pattern, -1, r->output_nl, NULL, NULL, 0, 0);
				if(!tinput)
					return -1;
				input_pattern = eval(tf, tinput, cwd, clen);
				free(tinput);
				if(!input_pattern)
					return -1;
				if(check_recursive_chain(tf, input_pattern, bl, r, ext, cwd, clen) < 0)
					return -1;
				delete_name_list(&r->order_only_inputs);
				if(parse_input_pattern(tf, input_pattern, &r->inputs,
						       &r->order_only_inputs, bl, r->line_number,
						       cwd, clen, 0) < 0)
					return -1;
				make_name_list_unique(&r->inputs);
				free(input_pattern);
				if(r->inputs.num_entries)
					break;
			}
			if(!r->inputs.num_entries) {
				fprintf(stderr, "Error: Unable to find any inputs for the *-chain for output '%s' in rule at line %i\n", last_output_pattern, r->line_number);
				return -1;
			}
		}

		list_for_each_entry(bal, banglist, list) {
			if(bal->list.next == banglist) {
				/* Apply the output pattern specified in the rule
				 * to the last !-macro in the *-chain.
				 */
				r->output_pattern = last_output_pattern;
			} else {
				if(strcmp(bal->br->output_pattern, "") == 0) {
					fprintf(stderr, "Error: Intermediate !-macro '%s' in *-chain '%s' does not specify an output pattern.\n", bal->br->st.s, ch->st.s);
					return -1;
				}
			}
			r->command = bal->br->st.s;
			r->command_len = bal->br->st.len;

			if(__execute_rule(tf, r, &output_nl, cwd, clen) < 0)
				return -1;

			delete_name_list(&r->inputs);
			move_name_list(&r->inputs, &output_nl);
		}
		delete_name_list(&r->inputs);
	} else {
		if(__execute_rule(tf, r, &output_nl, cwd, clen) < 0)
			return -1;
		delete_name_list(&output_nl);
	}

	return 0;
}

static int __execute_rule(struct tupfile *tf, struct rule *r,
			  struct name_list *output_nl, const char *cwd, int clen)
{
	struct name_list_entry *nle;
	int is_bang = 0;
	int foreach = 0;

	if(r->command[0] == '!') {
		struct string_tree *st;
		struct bang_rule *br;

		is_bang = 1;

		/* If we can't find the actual !-macro, it may be that there
		 * are only extension-specific macros, in which case the rule
		 * itself determines the foreach-ness.
		 */
		st = string_tree_search2(&tf->bang_tree, r->command, r->command_len, NULL);
		if(st) {
			br = container_of(st, struct bang_rule, st);
			foreach = br->foreach;
		}
	}
	/* Either the !-macro or the rule itself can list 'foreach'. There is no
	 * way to cancel a foreach in a !-macro.
	 */
	if(r->foreach)
		foreach = r->foreach;

	if(foreach) {
		struct name_list tmp_nl;
		struct name_list_entry tmp_nle;
		char *old_command = NULL;
		char *old_output_pattern = NULL;
		int old_command_len = 0;

		/* For a foreach loop, iterate over each entry in the rule's
		 * namelist and do a shallow copy over into a single-entry
		 * temporary namelist. Note that we cheat by not actually
		 * allocating a separate nle, which is why we don't have to do
		 * a delete_name_list_entry for the temporary list and can just
		 * reinitialize the pointers using init_name_list.
		 *
		 * TODO: Compare ext to previous ext to re-use the bang parsing?
		 * or would that be a confusing premature optimization?
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
				ext = tmp_nle.base + tmp_nle.extlessbaselen;
				extlen = tmp_nle.baselen - tmp_nle.extlessbaselen;
			}
			if(is_bang) {
				/* parse_bang_rule overwrites the command and
				 * output pattern, so save the old pointers to
				 * be restored after do_rule().
				 */
				old_command = r->command;
				old_command_len = r->command_len;
				old_output_pattern = r->output_pattern;
				if(parse_bang_rule(tf, r, &tmp_nl, ext, cwd, clen) < 0)
					return -1;
			}
			/* The extension in do_rule() does not include the
			 * leading '.'
			 */
			if(do_rule(tf, r, &tmp_nl, cwd, clen, ext+1, extlen-1, output_nl) < 0)
				return -1;

			if(is_bang) {
				r->command = old_command;
				r->command_len = old_command_len;
				r->output_pattern = old_output_pattern;
				r->bang_extra_outputs = NULL;
				delete_name_list(&r->bang_oo_inputs);
			}

			delete_name_list_entry(&r->inputs, nle);
		}
	} else {
		/* Only parse non-foreach rules if the namelist has some
		 * entries, or if there is no input listed. We don't want to
		 * generate a command if there is an input pattern but no
		 * entries match (for example, *.o inputs to ld %f with no
		 * object files). However, if you have no input but just a
		 * command (eg: you want to run a shell script), then we still
		 * want to do the rule for that case.
		 *
		 * Also note that we check that the original user string is
		 * empty (r->empty_input), not the eval'd string. This way if
		 * the user specifies the input as $(foo) and it evaluates to
		 * empty, we won't try to execute do_rule(). But an empty user
		 * string implies that no input is required.
		 */
		if((r->inputs.num_entries > 0 || r->empty_input)) {
			if(is_bang) {
				if(parse_bang_rule(tf, r, NULL, NULL, cwd, clen) < 0)
					return -1;
			}

			if(do_rule(tf, r, &r->inputs, cwd, clen, NULL, 0, output_nl) < 0)
				return -1;

			delete_name_list(&r->inputs);
		} else {
			/* t2066 - if the inputs evaluate to empty, see if
			 * the command was a bang rule. If so, then invoke the
			 * .EMPTY rule if one was specified. If no empty rule
			 * is specified, then no command is generated.
			 */
			if(is_bang) {
				int rc;

				rc = parse_empty_bang_rule(tf, r, cwd, clen);
				if(rc < 0)
					return -1;
				if(rc == 0) {
					if(do_rule(tf, r, &r->inputs, cwd, clen,
						   NULL, 0, output_nl) < 0)
						return -1;
				}
			}
		}
	}

	delete_name_list(&r->order_only_inputs);
	delete_name_list(&r->bang_oo_inputs);

	return 0;
}

static int execute_reverse_rule(struct tupfile *tf, struct rule *r,
				struct bin_list *bl, const char *cwd, int clen)
{
	LIST_HEAD(oplist);
	struct path_list *pl;
	char *eval_pattern;
	struct name_list tmp_nl;
	struct name_list_entry tmp_nle;

	if(!r->foreach) {
		fprintf(stderr, "Error: reverse rule must use 'foreach'\n");
		return -1;
	}
	if(!r->input_pattern) {
		fprintf(stderr, "Error: reverse rule must have input list\n");
		return -1;
	}
	eval_pattern = eval(tf, r->input_pattern, cwd, clen);
	if(!eval_pattern)
		return -1;

	if(get_path_list(eval_pattern, &oplist, tf->tupid, NULL, NULL) < 0)
		return -1;
	make_path_list_unique(&oplist);

	while(!list_empty(&oplist)) {
		struct rule tmpr;
		char *tinput;
		char *input_pattern;

		pl = list_entry(oplist.next, struct path_list, list);

		if(pl->path) {
			/* Things with paths (eg: foo/built-in.o) get skipped.
			 * This is pretty much hacked to get linux to work.
			 */
			goto out_skip;
		}

		init_name_list(&tmp_nl);
		tmp_nle.path = malloc(pl->pel->len + 1);
		if(!tmp_nle.path) {
			perror("malloc");
			return -1;
		}
		memcpy(tmp_nle.path, pl->pel->path, pl->pel->len);
		tmp_nle.path[pl->pel->len] = 0;
		tmp_nle.len = pl->pel->len;
		tmp_nle.extlesslen = tmp_nle.len - 1;
		while(tmp_nle.extlesslen > 0 && tmp_nle.path[tmp_nle.extlesslen] != '.')
			tmp_nle.extlesslen--;

		tmp_nle.tent = tup_db_create_node_part(tf->tupid, tmp_nle.path, -1,
						       TUP_NODE_GENERATED);
		if(!tmp_nle.tent)
			return -1;
		set_nle_base(&tmp_nle);
		add_name_list_entry(&tmp_nl, &tmp_nle);

		tinput = tup_printf(r->output_pattern, -1, &tmp_nl, NULL, NULL, 0, 0);
		if(!tinput)
			return -1;
		input_pattern = eval(tf, tinput, cwd, clen);
		free(tinput);
		if(!input_pattern)
			return -1;

		tmpr.foreach = 0;
		tmpr.input_pattern = input_pattern;
		tmpr.output_pattern = tmp_nle.path;
		tmpr.bin = r->bin;
		tmpr.command = r->command;
		tmpr.command_len = r->command_len;
		tmpr.empty_input = 0;
		tmpr.line_number = r->line_number;
		tmpr.output_nl = &tmp_nl;

		if(execute_rule(tf, &tmpr, bl, cwd, clen) < 0)
			return -1;
		free(input_pattern);
		free(tmp_nle.path);

out_skip:
		del_pl(pl);
	}
	free(eval_pattern);

	return 0;
}

static int check_recursive_chain(struct tupfile *tf, const char *input_pattern,
				 struct bin_list *bl,
				 struct rule *r, const char *ext,
				 const char *cwd, int clen)
{
	LIST_HEAD(inp_list);
	char *inp;
	struct path_list *pl;
	int extlen = strlen(ext);;

	inp = strdup(input_pattern);
	if(!inp) {
		perror("strdup");
		return -1;
	}

	if(get_path_list(inp, &inp_list, tf->tupid, NULL, NULL) < 0)
		return -1;
	make_path_list_unique(&inp_list);

	while(!list_empty(&inp_list)) {
		pl = list_entry(inp_list.next, struct path_list, list);

		if(pl->pel->len > extlen) {
			if(name_cmp(pl->pel->path + pl->pel->len - extlen, ext) == 0) {
				struct rule tmpr;
				char output_pattern[] = "";
				char *tinput;

				tinput = malloc(pl->pel->len + 1);
				if(!tinput) {
					perror("malloc");
					return -1;
				}
				memcpy(tinput, pl->pel->path, pl->pel->len);
				tinput[pl->pel->len] = 0;

				tmpr.foreach = 1;
				tmpr.input_pattern = tinput;
				tmpr.output_pattern = output_pattern;
				tmpr.bin = NULL;
				tmpr.command = r->command;
				tmpr.command_len = r->command_len;
				tmpr.empty_input = 0;
				tmpr.line_number = r->line_number;
				tmpr.output_nl = NULL;
				if(execute_reverse_rule(tf, &tmpr, bl, cwd, clen) < 0)
					return -1;
				free(tinput);
			}
		}

		del_pl(pl);
	}
	free(inp);
	return 0;
}

static int input_pattern_to_nl(struct tupfile *tf, char *p,
			       struct name_list *nl, struct bin_list *bl,
			       int lno, int required)
{
	LIST_HEAD(plist);
	struct rb_root symtree = RB_ROOT;
	struct rb_node *rbn;
	struct tup_entry *tent;
	struct path_list *pl, *tmp;
	int sym_bork = 0;
	tupid_t symid;

	if(get_path_list(p, &plist, tf->tupid, bl, &symtree) < 0)
		return -1;
	tupid_tree_for_each(symid, rbn, &symtree) {
		tent = tup_entry_get(symid);
		if(tent->type == TUP_NODE_GENERATED) {
			fprintf(stderr, "Error: Attempted to input files using a symlink that was generated by tup. Directory %lli, symlink used was %lli, line number %i\n", tf->tupid, tent->tnode.tupid, lno);
			sym_bork = 1;
		}
	}
	if(sym_bork)
		return -1;
	free_tupid_tree(&symtree);

	if(parse_dependent_tupfiles(&plist, tf, tf->g) < 0)
		return -1;
	if(get_name_list(tf, &plist, nl, required) < 0)
		return -1;
	list_for_each_entry_safe(pl, tmp, &plist, list) {
		del_pl(pl);
	}
	return 0;
}

static int get_path_list(char *p, struct list_head *plist, tupid_t dt,
			 struct bin_list *bl, struct rb_root *symtree)
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
			struct pel_group pg;
			/* Path */
			pl->path = p;

			if(get_path_elements(p, &pg) < 0)
				return -1;
			if(pg.pg_flags & PG_HIDDEN) {
				fprintf(stderr, "Error: You specified a path '%s' that contains a hidden filename (since it begins with a '.' character). Tup ignores these files - please remove references to it from the Tupfile.\n", p);
				return -1;
			}
			pl->dt = find_dir_tupid_dt_pg(dt, &pg, &pl->pel, NULL, symtree, 0);
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

static void make_path_list_unique(struct list_head *plist)
{
	/* When make_name_list_unique can't be used, this can make a path_list
	 * unique in O(n^2) time.
	 */
	struct path_list *pl;
	struct path_list *tmp;

	list_for_each_entry_safe(pl, tmp, plist, list) {
		struct path_list *pl2;
		struct list_head *tlist;

		tlist = pl->list.next;
		while(tlist != plist) {
			pl2 = list_entry(tlist, struct path_list, list);
			if(pl->pel->len == pl2->pel->len &&
			   memcmp(pl->pel->path, pl2->pel->path, pl->pel->len) == 0) {
				del_pl(pl);
				break;
			}
			tlist = pl2->list.next;
		}
	}
}

static void del_pl(struct path_list *pl)
{
	list_del(&pl->list);
	free(pl->pel);
	free(pl);
}

static void make_name_list_unique(struct name_list *nl)
{
	struct name_list_entry *tmp;
	struct list_head *input_list;
	struct name_list_entry *nle;

	/* Use the tup entry list as an easy cheat to remove duplicates. Only
	 * care about dups in the inputs namelist, since the others are just
	 * added to the tupid_tree and aren't used in %-flags.
	 *
	 * The trick here is that we need to prune duplicate inputs, but still
	 * maintain the order. So we can't stick the input tupids in a tree and
	 * use that, since that would kill the order. Also, just going through
	 * the linked list twice would be O(n^2), which would suck. Since the
	 * tup_entry's are already unique, we can use the entry list to
	 * determine if the nle is already present or not. If it is already
	 * present, the second and further duplicates will be removed.
	 */
	input_list = tup_entry_get_list();
	list_for_each_entry_safe(nle, tmp, &nl->entries, list) {
		if(tup_entry_in_list(nle->tent)) {
			delete_name_list_entry(nl, nle);
		} else {
			tup_entry_list_add(nle->tent, input_list);
		}
	}
	tup_entry_release_list();
}

static int parse_dependent_tupfiles(struct list_head *plist, struct tupfile *tf,
				    struct graph *g)
{
	struct path_list *pl;

	list_for_each_entry(pl, plist, list) {
		/* Only care about non-bins, and directories that are not our
		 * own.
		 */
		if(!pl->bin && pl->dt != tf->tupid) {
			struct node *n;

			n = find_node(g, pl->dt);
			if(n != NULL && !n->already_used) {
				n->already_used = 1;
				if(parse(n, g) < 0)
					return -1;
			}
			if(tupid_tree_add_dup(&tf->input_tree, pl->dt) < 0)
				return -1;
		}
	}
	return 0;
}

static int get_name_list(struct tupfile *tf, struct list_head *plist,
			 struct name_list *nl, int required)
{
	struct path_list *pl;

	list_for_each_entry(pl, plist, list) {
		if(pl->bin) {
			if(nl_add_bin(pl->bin, nl) < 0)
				return -1;
		} else {
			if(nl_add_path(tf, pl, nl, required) < 0)
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
		       struct name_list *nl, int required)
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
	if(char_find(pl->pel->path, pl->pel->len, "*?[") == 0) {
		struct tup_entry *tent;

		if(tup_db_select_tent_part(pl->dt, pl->pel->path, pl->pel->len, &tent) < 0) {
			return -1;
		}
		if(!tent) {
			if(!required)
				return 0;
			fprintf(stderr, "Error: Explicitly named file '%.*s' not found in subdir %lli.\n", pl->pel->len, pl->pel->path, pl->dt);
			tup_db_print(stderr, pl->dt);
			return -1;
		}
		if(tent->type == TUP_NODE_GHOST) {
			if(!required)
				return 0;
			fprintf(stderr, "Error: Explicitly named file '%.*s' is a ghost file, so it can't be used as an input.\n", pl->pel->len, pl->pel->path);
			tup_db_print(stderr, tent->tnode.tupid);
			return -1;
		}
		if(tupid_tree_search(&tf->g->delete_tree, tent->tnode.tupid) != NULL) {
			if(!required)
				return 0;
			fprintf(stderr, "Error: Explicitly named file '%.*s' in subdir %lli is scheduled to be deleted (possibly the command that created it has been removed).\n", pl->pel->len, pl->pel->path, pl->dt);
			tup_db_print(stderr, pl->dt);
			return -1;
		}
		if(build_name_list_cb(&args, tent) < 0)
			return -1;
	} else {
		if(tup_db_select_node_dir_glob(build_name_list_cb, &args, pl->dt, pl->pel->path, pl->pel->len, &tf->g->delete_tree) < 0)
			return -1;
	}
	return 0;
}

static int nl_add_bin(struct bin *b, struct name_list *nl)
{
	struct bin_entry *be;
	struct name_list_entry *nle;
	int extlesslen;

	list_for_each_entry(be, &b->entries, list) {
		extlesslen = be->len - 1;
		while(extlesslen > 0 && be->path[extlesslen] != '.')
			extlesslen--;
		if(extlesslen == 0)
			extlesslen = be->len;

		nle = malloc(sizeof *nle);
		if(!nle) {
			perror("malloc");
			return -1;
		}

		nle->path = malloc(be->len + 1);
		if(!nle->path) {
			free(nle);
			return -1;
		}
		memcpy(nle->path, be->path, be->len+1);

		nle->len = be->len;
		nle->extlesslen = extlesslen;
		nle->tent = be->tent;
		set_nle_base(nle);

		add_name_list_entry(nl, nle);
	}
	return 0;
}

static int build_name_list_cb(void *arg, struct tup_entry *tent)
{
	struct build_name_list_args *args = arg;
	int extlesslen;
	int len;
	struct name_list_entry *nle;

	len = tent->name.len + args->dirlen;
	extlesslen = tent->name.len - 1;
	while(extlesslen > 0 && tent->name.s[extlesslen] != '.')
		extlesslen--;
	if(extlesslen == 0)
		extlesslen = tent->name.len;
	extlesslen += args->dirlen;

	nle = malloc(sizeof *nle);
	if(!nle) {
		perror("malloc");
		return -1;
	}

	nle->path = set_path(tent->name.s, args->dir, args->dirlen);
	if(!nle->path) {
		free(nle);
		return -1;
	}

	nle->len = len;
	nle->extlesslen = extlesslen;
	nle->tent = tent;
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
		path[dirlen-1] = PATH_SEP;
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

static int find_existing_command(const struct name_list *onl,
				 struct rb_root *del_tree,
				 tupid_t *cmdid)
{
	struct name_list_entry *onle;
	list_for_each_entry(onle, &onl->entries, list) {
		int rc;
		tupid_t incoming;

		rc = tup_db_get_incoming_link(onle->tent->tnode.tupid, &incoming);
		if(rc < 0)
			return -1;
		/* Only want commands that are still in the del_tree. Any
		 * command not in the del_tree will mean it has already been
		 * parsed, and so will probably cause an error later in the
		 * duplicate link check.
		 */
		if(incoming != -1) {
			if(tupid_tree_search(del_tree, incoming) != NULL) {
				*cmdid = incoming;
				return 0;
			}
		}
	}
	*cmdid = -1;
	return 0;
}

static int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
		   const char *cwd, int clen, const char *ext, int extlen,
		   struct name_list *output_nl)
{
	struct name_list onl;
	struct name_list extra_onl;
	struct name_list_entry *nle, *onle;
	char *output_pattern;
	char *extra_pattern = NULL;
	char *tcmd;
	char *cmd;
	struct path_list *pl;
	struct tupid_tree *cmd_tt;
	tupid_t cmdid = -1;
	LIST_HEAD(oplist);
	struct rb_root tree = {NULL};
	int extra_outputs = 0;
	char sep[] = "|";
	struct tup_entry *tmptent = NULL;

	/* t3017 - empty rules are just pass-through to get the input into the
	 * bin.
	 */
	if(r->command_len == 0) {
		if(r->bin) {
			list_for_each_entry(nle, &nl->entries, list) {
				if(bin_add_entry(r->bin, nle->path, nle->len, nle->tent) < 0)
					return -1;
			}
		}
		return 0;
	}

	init_name_list(&onl);
	init_name_list(&extra_onl);

	output_pattern = eval(tf, r->output_pattern, cwd, clen);
	if(!output_pattern)
		return -1;
	if(get_path_list(output_pattern, &oplist, tf->tupid, NULL, NULL) < 0)
		return -1;
	if(r->bang_extra_outputs) {
		/* Insert a fake separator in case the rule doesn't have one */
		if(get_path_list(sep, &oplist, tf->tupid, NULL, NULL) < 0)
			return -1;
		extra_pattern = eval(tf, r->bang_extra_outputs, cwd, clen);
		if(!extra_pattern)
			return -1;
		if(get_path_list(extra_pattern, &oplist, tf->tupid, NULL, NULL) < 0)
			return -1;
	}
	while(!list_empty(&oplist)) {
		struct name_list *use_onl;
		pl = list_entry(oplist.next, struct path_list, list);

		if(pl->path) {
			fprintf(stderr, "Error: Attempted to create an output file '%s', which contains a '/' character. Tupfiles should only output files in their own directories.\n - Directory: %lli\n - Rule at line %i: [35m%s[0m\n", pl->path, tf->tupid, r->line_number, r->command);
			return -1;
		}
		if(pl->pel->len == 1 && pl->pel->path[0] == '|') {
			extra_outputs = 1;
			goto out_pl;
		}

		onle = malloc(sizeof *onle);
		if(!onle) {
			perror("malloc");
			return -1;
		}

		/* tup_printf allows %O if we have an onl and are not a command */
		if(extra_outputs) {
			use_onl = &onl;
		} else {
			use_onl = NULL;
		}
		onle->path = tup_printf(pl->pel->path, pl->pel->len, nl, use_onl, NULL, 0, 0);
		if(!onle->path) {
			free(onle);
			return -1;
		}
		if(strchr(onle->path, '/')) {
			/* Same error as above...uhh, I guess I should rework
			 * this.
			 */
			fprintf(stderr, "Error: Attempted to create an output file '%s', which contains a '/' character. Tupfiles should only output files in their own directories.\n - Directory: %lli\n - Rule at line %i: [35m%s[0m\n", onle->path, tf->tupid, r->line_number, r->command);
			free(onle->path);
			free(onle);
			return -1;
		}
		if(name_cmp(onle->path, "Tupfile") == 0 ||
		   name_cmp(onle->path, "Tuprules.tup") == 0 ||
		   name_cmp(onle->path, "tup.config") == 0) {
			fprintf(stderr, "Error: Attempted to generate a file called '%s', which is reserved by tup. Your build configuration must be comprised of files you write yourself.\n", onle->path);
			free(onle->path);
			free(onle);
			return -1;
		}
		onle->len = strlen(onle->path);
		onle->extlesslen = onle->len - 1;
		while(onle->extlesslen > 0 && onle->path[onle->extlesslen] != '.')
			onle->extlesslen--;

		onle->tent = tup_db_create_node_part(tf->tupid, onle->path, -1,
						     TUP_NODE_GENERATED);
		if(!onle->tent) {
			free(onle->path);
			free(onle);
			return -1;
		}

		set_nle_base(onle);
		if(extra_outputs) {
			add_name_list_entry(&extra_onl, onle);
		} else {
			add_name_list_entry(&onl, onle);

			if(r->bin) {
				if(bin_add_entry(r->bin, onle->path, onle->len, onle->tent) < 0)
					return -1;
			}
		}

out_pl:
		del_pl(pl);
	}
	/* Has to be freed after use of oplist */
	free(output_pattern);
	free(extra_pattern);

	tcmd = tup_printf(r->command, -1, nl, &onl, ext, extlen, 1);
	if(!tcmd)
		return -1;
	cmd = eval(tf, tcmd, cwd, clen);
	if(!cmd)
		return -1;
	free(tcmd);

	/* If we already have our command string in the db, then use that.
	 * Otherwise, we try to find an existing command of a different
	 * name that points to the output files we are trying to create.
	 * If neither of those cases apply, we just create a new command
	 * node.
	 */
	if(tup_db_select_tent(tf->tupid, cmd, &tmptent) < 0)
		return -1;
	if(tmptent) {
		cmdid = tmptent->tnode.tupid;
	} else {
		if(find_existing_command(&onl, &tf->g->delete_tree, &cmdid) < 0)
			return -1;
		if(cmdid == -1) {
			cmdid = create_command_file(tf->tupid, cmd);
		} else {
			if(tup_db_set_name(cmdid, cmd) < 0)
				return -1;
		}
	}

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
	tree_entry_remove(&tf->g->delete_tree, cmdid, &tf->g->delete_count);

	while(!list_empty(&onl.entries)) {
		onle = list_entry(onl.entries.next, struct name_list_entry,
				  list);
		if(tup_db_create_unique_link(cmdid, onle->tent->tnode.tupid, &tf->g->delete_tree, &tree) < 0) {
			fprintf(stderr, "You may have multiple commands trying to create file '%s'\n", onle->path);
			return -1;
		}
		tree_entry_remove(&tf->g->delete_tree, onle->tent->tnode.tupid,
				  &tf->g->delete_count);
		move_name_list_entry(output_nl, &onl, onle);
	}

	while(!list_empty(&extra_onl.entries)) {
		onle = list_entry(extra_onl.entries.next, struct name_list_entry,
				  list);
		if(tup_db_create_unique_link(cmdid, onle->tent->tnode.tupid, &tf->g->delete_tree, &tree) < 0) {
			fprintf(stderr, "You may have multiple commands trying to create file '%s'\n", onle->path);
			return -1;
		}
		tree_entry_remove(&tf->g->delete_tree, onle->tent->tnode.tupid,
				  &tf->g->delete_count);
		delete_name_list_entry(&extra_onl, onle);
	}

	if(tup_db_write_outputs(cmdid, &tree) < 0)
		return -1;
	free_tupid_tree(&tree);

	list_for_each_entry(nle, &nl->entries, list) {
		if(tupid_tree_add_dup(&tree, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	list_for_each_entry(nle, &r->order_only_inputs.entries, list) {
		if(tupid_tree_add_dup(&tree, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	list_for_each_entry(nle, &r->bang_oo_inputs.entries, list) {
		if(tupid_tree_add_dup(&tree, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	if(tup_db_write_inputs(cmdid, &tree, &tf->g->delete_tree) < 0)
		return -1;
	free_tupid_tree(&tree);
	return 0;
}

static void init_name_list(struct name_list *nl)
{
	INIT_LIST_HEAD(&nl->entries);
	nl->num_entries = 0;
	nl->totlen = 0;
	nl->basetotlen = 0;
	nl->extlessbasetotlen = 0;
}

static void set_nle_base(struct name_list_entry *nle)
{
	nle->base = nle->path + nle->len;
	nle->baselen = 0;
	while(nle->base > nle->path) {
		nle->base--;
		if(nle->base[0] == PATH_SEP) {
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
	nl->basetotlen += nle->baselen;
	nl->extlessbasetotlen += nle->extlessbaselen;
}

static void delete_name_list(struct name_list *nl)
{
	struct name_list_entry *nle;
	while(!list_empty(&nl->entries)) {
		nle = list_entry(nl->entries.next, struct name_list_entry,
				 list);
		delete_name_list_entry(nl, nle);
	}
}

static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle)
{
	nl->num_entries--;
	nl->totlen -= nle->len;
	nl->basetotlen -= nle->baselen;
	nl->extlessbasetotlen -= nle->extlessbaselen;

	list_del(&nle->list);
	free(nle->path);
	free(nle);
}

static void move_name_list_entry(struct name_list *newnl, struct name_list *oldnl,
				 struct name_list_entry *nle)
{
	oldnl->num_entries--;
	oldnl->totlen -= nle->len;
	oldnl->basetotlen -= nle->baselen;
	oldnl->extlessbasetotlen -= nle->extlessbaselen;

	list_del(&nle->list);

	newnl->num_entries++;
	newnl->totlen += nle->len;
	newnl->basetotlen += nle->baselen;
	newnl->extlessbasetotlen += nle->extlessbaselen;
	list_add_tail(&nle->list, &newnl->entries);
}

static void move_name_list(struct name_list *newnl, struct name_list *oldnl)
{
	struct name_list_entry *nle;

	while(!list_empty(&oldnl->entries)) {
		nle = list_entry(oldnl->entries.next, struct name_list_entry, list);
		move_name_list_entry(newnl, oldnl, nle);
	}
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
			struct name_list *onl, const char *ext, int extlen,
			int is_command)
{
	struct name_list_entry *nle;
	char *s;
	int x;
	const char *p;
	const char *next;
	int clen = strlen(cmd);

	if(!nl) {
		fprintf(stderr, "tup internal error: tup_printf called with NULL name_list\n");
		return NULL;
	}

	if(cmd_len == -1) {
		cmd_len = strlen(cmd);
	}
	clen = cmd_len;

	p = cmd;
	while((next = find_char(p, cmd+cmd_len - p, '%')) !=  NULL) {
		int space_chars;

		clen -= 2;
		if(next == cmd+cmd_len-1) {
			fprintf(stderr, "Error: Unfinished %%-flag at the end of the string '%s'\n", cmd);
			return NULL;
		}
		next++;
		p = next+1;
		space_chars = nl->num_entries - 1;
		if(*next == 'f') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%f used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->totlen + space_chars;
		} else if(*next == 'b') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%b used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->basetotlen + space_chars;
		} else if(*next == 'B') {
			if(nl->num_entries == 0) {
				fprintf(stderr, "Error: %%B used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->extlessbasetotlen + space_chars;
		} else if(*next == 'e') {
			if(!ext) {
				fprintf(stderr, "Error: %%e is only valid with a foreach rule for files that have extensions.\n");
				if(nl->num_entries == 1) {
					nle = list_entry(nl->entries.next,
							 struct name_list_entry,
							 list);
					fprintf(stderr, " -- Path: '%s'\n", nle->path);
				} else {
					fprintf(stderr, " -- This does not appear to be a foreach rule\n");
				}
				return NULL;
			}
			clen += extlen;
		} else if(*next == 'o') {
			if(!is_command) {
				fprintf(stderr, "Error: %%o can only be used in a command.\n");
				return NULL;
			}
			if(onl->num_entries == 0) {
				fprintf(stderr, "Error: %%o used in rule pattern and no output files were specified.\n");
				return NULL;
			}
			clen += onl->totlen + (onl->num_entries-1);
		} else if(*next == 'O') {
			if(!onl) {
				fprintf(stderr, "Error: %%O can only be used in the extra outputs section.\n");
				return NULL;
			}
			if(onl->num_entries != 1) {
				fprintf(stderr, "Error: %%O can only be used if there is exactly one output specified.\n");
				return NULL;
			}
			clen += onl->extlessbasetotlen;
		} else if(*next == '%') {
			clen++;
		} else {
			fprintf(stderr, "Error: Unknown %%-flag: '%c'\n", *next);
			return NULL;
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
		if(*next == 'f') {
			int first = 1;
			list_for_each_entry(nle, &nl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->path, nle->len);
				x += nle->len;
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
				first = 0;
			}
		} else if(*next == 'e') {
			memcpy(&s[x], ext, extlen);
			x += extlen;
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
		} else if(*next == 'O') {
			nle = list_entry(onl->entries.next, struct name_list_entry, list);
			memcpy(&s[x], nle->path, nle->extlessbaselen);
			x += nle->extlessbaselen;
		} else if(*next == '%') {
			s[x] = '%';
			x++;
		} else {
			fprintf(stderr, "tup internal error: Unhandled %%-flag '%c'\n", *next);
			return NULL;
		}
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
				/* \@( becomes @( */
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
				} else if(rparen - var > 7 &&
					  strncmp(var, "CONFIG_", 7) == 0) {
					const char *atvar;
					atvar = var+7;
					vlen = tup_db_get_varlen(atvar, rparen-atvar);
					if(vlen < 0)
						return NULL;
					len += vlen;
					s = rparen + 1;
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
				/* \@( becomes @( */
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
				} else if(rparen - var > 7 &&
					  strncmp(var, "CONFIG_", 7) == 0) {
					const char *atvar;
					struct tup_entry *tent;
					atvar = var+7;

					tent = tup_db_get_var(atvar, rparen-atvar, &p);
					if(!tent)
						return NULL;
					if(tupid_tree_add_dup(&tf->input_tree, tent->tnode.tupid) < 0)
						return NULL;
					s = rparen + 1;
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
			struct tup_entry *tent;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					expected = "ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				tent = tup_db_get_var(var, rparen-var, &p);
				if(!tent)
					return NULL;
				if(tupid_tree_add_dup(&tf->input_tree, tent->tnode.tupid) < 0)
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
