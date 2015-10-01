/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2015  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* _ATFILE_SOURCE needed at least on linux x86_64 */
#define _ATFILE_SOURCE
#include "parser.h"
#include "luaparser.h"
#include "progress.h"
#include "fileio.h"
#include "fslurp.h"
#include "db.h"
#include "environ.h"
#include "graph.h"
#include "config.h"
#include "bin.h"
#include "entry.h"
#include "container.h"
#include "if_stmt.h"
#include "server.h"
#include "variant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#define SYNTAX_ERROR -2
#define CIRCULAR_DEPENDENCY_ERROR -3
#define ERROR_DIRECTIVE_ERROR -4

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
	TAILQ_ENTRY(bang_list) list;
	struct bang_rule *br;
};
TAILQ_HEAD(bang_list_head, bang_list);

struct src_chain {
	TAILQ_ENTRY(src_chain) list;
	char *input_pattern;
	struct bang_list_head banglist;
};
TAILQ_HEAD(src_chain_head, src_chain);

struct chain {
	struct string_tree st;
	struct src_chain_head src_chain_list;
	struct bang_list_head banglist;
};

struct build_name_list_args {
	struct name_list *nl;
	const char *dir;
	int dirlen;
	const char *globstr;  /* Pointer to the basename of the filename in the tupfile */
	int globstrlen;       /* Length of the basename */
	int wildcard;
	struct tupfile *tf;
};

static int open_lua_tupfile(struct tupfile *tf, struct tup_entry *tent,
			    char *luafilename);
static int parse_tupfile(struct tupfile *tf, struct buf *b, const char *filename);
static int var_ifdef(struct tupfile *tf, const char *var);
static int eval_eq(struct tupfile *tf, char *expr, char *eol);
static int error_directive(struct tupfile *tf, char *cmdline);
static int include_rules(struct tupfile *tf);
static int preload(struct tupfile *tf, char *cmdline);
static int run_script(struct tupfile *tf, char *cmdline, int lno,
		      struct bin_head *bl);
static int gitignore(struct tupfile *tf, tupid_t dt);
static int check_toplevel_gitignore(struct tupfile *tf);
static int include_file(struct tupfile *tf, const char *file);
static int parse_rule(struct tupfile *tf, char *p, int lno, struct bin_head *bl);
static int parse_bang_definition(struct tupfile *tf, char *p, int lno);
static int parse_chain_definition(struct tupfile *tf, char *p, int lno);
static int set_variable(struct tupfile *tf, char *line);
static int parse_empty_bang_rule(struct tupfile *tf, struct rule *r);
static int parse_bang_rule(struct tupfile *tf, struct rule *r,
			   struct name_list *nl, const char *ext, int extlen);
static void free_bang_rule(struct string_entries *root, struct bang_rule *br);
static void free_bang_tree(struct string_entries *root);
static void free_chain_tree(struct string_entries *root);
static void free_banglist(struct bang_list_head *head);
static void free_dir_lists(struct string_entries *root);
static int split_input_pattern(struct tupfile *tf, char *p, char **o_input,
			       char **o_cmd, int *o_cmdlen, char **o_output,
			       char **o_bin, int *swapio);
static int parse_input_pattern(struct tupfile *tf, char *input_pattern,
			       struct name_list *inputs,
			       struct name_list *order_only_inputs,
			       struct bin_head *bl, int required);
static int execute_rule(struct tupfile *tf, struct rule *r, struct bin_head *bl);
static int execute_rule_internal(struct tupfile *tf, struct rule *r,
				 struct name_list *output_nl);
static int execute_reverse_rule(struct tupfile *tf, struct rule *r,
				struct bin_head *bl);
static int check_recursive_chain(struct tupfile *tf, const char *input_pattern,
				 struct bin_head *bl,
				 struct rule *r, const char *ext);
static int input_pattern_to_nl(struct tupfile *tf, char *p,
			       struct name_list *nl, struct bin_head *bl,
			       int required);
static int get_path_list(struct tupfile *tf, char *p, struct path_list_head *plist,
			 tupid_t dt, struct bin_head *bl, int create_output_dirs);
static void make_path_list_unique(struct path_list_head *plist);
static void make_name_list_unique(struct name_list *nl);
static int nl_add_path(struct tupfile *tf, struct path_list *pl,
		       struct name_list *nl, int required);
static int nl_add_bin(struct bin *b, struct name_list *nl);
static int build_name_list_cb(void *arg, struct tup_entry *tent);
static char *set_path(const char *name, const char *dir, int dirlen);
static int do_rule_output_pattern(struct tupfile *tf, struct rule *r,
				  struct name_list *nl, const char *ext, int extlen,
				  struct name_list *output_nl);
static void set_nle_base(struct name_list_entry *nle);
static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle);
static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle);
static void move_name_list_entry(struct name_list *newnl, struct name_list *oldnl,
				 struct name_list_entry *nle);
static void move_name_list(struct name_list *newnl, struct name_list *oldnl);
static char *tup_printf(struct tupfile *tf, const char *cmd, int cmd_len,
			struct name_list *nl, struct name_list *onl,
			const char *ext, int extlen,
			const char *extra_command);
static char *eval(struct tupfile *tf, const char *string, int allow_nodes);

static int glob_parse(const char *base, int baselen, char *expanded, int *globidx);

static int debug_run = 0;

void parser_debug_run(void)
{
	debug_run = 1;
	lua_parser_debug_run();
}

int parse(struct node *n, struct graph *g, struct timespan *retts, int refactoring, int use_server)
{
	struct tupfile tf;
	int fd;
	int rc = -1;
	int parser_lua = 0;
	struct buf b = {NULL, 0};
	struct parser_server ps;
	struct timeval orig_start;
	char luafilename[PATH_MAX];

	timespan_start(&tf.ts);
	memcpy(&orig_start, &tf.ts.start, sizeof(orig_start));
	if(n->parsing) {
		fprintf(stderr, "tup error: Circular dependency found among Tupfiles. Last directory: ");
		print_tup_entry(stderr, n->tent);
		fprintf(stderr, "\n");
		return CIRCULAR_DEPENDENCY_ERROR;
	}
	n->parsing = 1;

	tf.variant = tup_entry_variant(n->tent);
	tf.ps = &ps;
	tf.f = tmpfile();
	if(!tf.f) {
		perror("tmpfile");
		return -1;
	}
	tf.ls = NULL;
	tf.luaerror = TUPLUA_NOERROR;
	if(n->tent->type == TUP_NODE_GHOST) {
		tf.use_server = 0;
	} else {
		tf.use_server = use_server;
	}

	/* We may need to convert normal dirs back to generated dirs,
	 * so add this one to check.
	 */
	if(tf.variant->root_variant) {
		if(tupid_tree_add_dup(&g->normal_dir_root, n->tent->tnode.tupid) < 0)
			return -1;
	}

	init_file_info(&ps.s.finfo, tf.variant->variant_dir);
	ps.s.id = n->tnode.tupid;
	pthread_mutex_init(&ps.lock, NULL);

	RB_INIT(&tf.cmd_root);
	RB_INIT(&tf.env_root);
	RB_INIT(&tf.bang_root);
	RB_INIT(&tf.input_root);
	RB_INIT(&tf.chain_root);
	RB_INIT(&tf.directory_root);
	RB_INIT(&ps.directories);

	if(refactoring) {
		RB_INIT(&tf.refactoring_cmd_delete_root);
	}

	if(tf.use_server) {
		if(server_parser_start(&ps) < 0)
			return -1;
	} else {
		ps.root_fd = tup_top_fd();
	}

	tf.tupid = n->tnode.tupid;
	tf.curtent = tup_entry_get(tf.tupid);
	tf.root_fd = ps.root_fd;
	tf.g = g;
	tf.refactoring = refactoring;
	if(tf.variant->root_variant) {
		tf.srctent = NULL;
	} else {
		if(variant_get_srctent(tf.variant, tf.tupid, &tf.srctent) < 0)
			goto out_server_stop;
	}
	tf.ign = 0;
	tf.circular_dep_error = 0;
	if(nodedb_init(&tf.node_db) < 0)
		goto out_server_stop;
	if(vardb_init(&tf.vdb) < 0)
		goto out_close_node_db;
	if(environ_add_defaults(&tf.env_root) < 0)
		goto out_close_vdb;

	/* Keep track of the commands and generated files that we had created
	 * previously. We'll check these against the new ones in order to see
	 * if any should be removed.
	 */
	if(tup_db_dirtype_to_tree(tf.tupid, &g->cmd_delete_root, &g->cmd_delete_count, TUP_NODE_CMD) < 0)
		goto out_close_vdb;
	if(tup_db_srcid_to_tree(tf.tupid, &g->gen_delete_root, &g->gen_delete_count, TUP_NODE_GENERATED) < 0)
		goto out_close_vdb;

	if(refactoring) {
		if(tup_db_dirtype_to_tree(tf.tupid, &tf.refactoring_cmd_delete_root, NULL, TUP_NODE_CMD) < 0)
			goto out_close_vdb;
	}

	if(n->tent->type == TUP_NODE_GHOST) {
		fd = -1;
		tf.cur_dfd = -1;
	} else {
		tf.cur_dfd = tup_entry_openat(ps.root_fd, n->tent);
		if(tf.cur_dfd < 0) {
			fprintf(tf.f, "tup error: Unable to open directory ID %lli\n", tf.tupid);
			goto out_close_vdb;
		}

		fd = openat(tf.cur_dfd, "Tupfile", O_RDONLY);
		if(fd < 0) {
			if(errno == ENOENT) {
				parser_lua = 1;
				fd = open_lua_tupfile(&tf, n->tent, luafilename);
				if(fd < 0) {
					if(errno == ENOENT) {
						/* No Tupfile means we have nothing to do */
						if(n->tent->tnode.tupid == DOT_DT) {
							/* Check to see if the top-level rules file would .gitignore. We disable
							 * tf.tupid so no rules get created.
							 */
							if(check_toplevel_gitignore(&tf) < 0)
								goto out_close_dfd;
						}
					} else {
						parser_error(&tf, luafilename);
						goto out_close_dfd;
					}
				}
			} else {
				parser_error(&tf, "Tupfile");
				goto out_close_dfd;
			}
		}
	}
	if(fd >= 0) {
		if(fslurp_null(fd, &b) < 0)
			goto out_close_file;
		if(!parser_lua) {
			if(parse_tupfile(&tf, &b, "Tupfile") < 0)
				goto out_free_bs;
		} else {
			if(parse_lua_tupfile(&tf, &b, luafilename) < 0)
				goto out_free_bs;
		}
	}
	if(tf.ign) {
		if(!tf.variant->root_variant) {
			if(n->tent->srcid == DOT_DT) {
				if(gitignore(&tf, n->tent->srcid) < 0) {
					goto out_free_bs;
				}
			}
		}
		if(gitignore(&tf, tf.tupid) < 0) {
			goto out_free_bs;
		}
	} else {
		if(refactoring) {
			struct tup_entry *tent;
			if(tup_db_select_tent(tf.tupid, ".gitignore", &tent) < 0)
				goto out_free_bs;
			if(tent && tent->type == TUP_NODE_GENERATED) {
				fprintf(tf.f, "tup refactoring error: Attempting to remove the .gitignore file.\n");
				goto out_free_bs;
			}
		}
	}
	rc = 0;
out_free_bs:
	free(b.s);
out_close_file:
	if(fd >= 0 && close(fd) < 0) {
		parser_error(&tf, "close(fd)");
		rc = -1;
	}
out_close_dfd:
	if(tf.cur_dfd >= 0 && close(tf.cur_dfd) < 0) {
		parser_error(&tf, "close(tf.cur_dfd)");
		rc = -1;
	}
out_close_vdb:
	if(vardb_close(&tf.vdb) < 0)
		rc = -1;
out_close_node_db:
	if(nodedb_close(&tf.node_db) < 0)
		rc = -1;
out_server_stop:
	if(tf.use_server)
		if(server_parser_stop(&ps) < 0)
			rc = -1;

	if(rc == 0) {
		if(refactoring) {
			struct tupid_tree *tt;
			struct tup_entry *tent;

			RB_FOREACH(tt, tupid_entries, &tf.refactoring_cmd_delete_root) {
				rc = -1;
				if(tup_entry_add(tt->tupid, &tent) < 0)
					return -1;
				fprintf(tf.f, "tup refactoring error: Attempting to delete a command: ");
				print_tup_entry(tf.f, tent);
				fprintf(tf.f, "\n");
			}
		}
		if(add_parser_files(tf.f, &ps.s.finfo, &tf.input_root, tf.variant->tent->tnode.tupid) < 0)
			rc = -1;
		if(tup_db_write_dir_inputs(tf.f, tf.tupid, &tf.input_root) < 0)
			rc = -1;
	}

	pthread_mutex_lock(&ps.lock);
	free_dir_lists(&ps.directories);
	pthread_mutex_unlock(&ps.lock);

	free_chain_tree(&tf.chain_root);
	free_tupid_tree(&tf.env_root);
	free_tupid_tree(&tf.cmd_root);
	free_tupid_tree(&tf.directory_root);
	free_bang_tree(&tf.bang_root);
	free_tupid_tree(&tf.input_root);

	timespan_end(&tf.ts);
	if(retts) {
		/* Report back the original start time, and our real end time.
		 * This accounts for the total time to parse us, plus any
		 * dependent Tupfiles. If another Tupfile depends on us, they
		 * can subtract this full time out from their reported time.
		 */
		memcpy(&retts->start, &orig_start, sizeof(retts->start));
		memcpy(&retts->end, &tf.ts.end, sizeof(retts->end));
	}
	show_result(n->tent, rc != 0, &tf.ts, NULL, 0);
	if(fflush(tf.f) != 0) {
		/* Use perror, since we're trying to flush the tf.f output */
		perror("fflush");
		rc = -1;
	}
	rewind(tf.f);
	display_output(fileno(tf.f), rc == 0 ? 0 : 3, NULL, 0, NULL);
	if(fclose(tf.f) != 0) {
		/* Use perror, since we're trying to close the tf.f output */
		perror("fclose");
		rc = -1;
	}
	if(tf.circular_dep_error)
		rc = CIRCULAR_DEPENDENCY_ERROR;

	return rc;
}

static int open_lua_tupfile(struct tupfile *tf, struct tup_entry *tent,
			    char *luafilename)
{
	int fd;
	int n = 0;

	fd = openat(tf->cur_dfd, "Tupfile.lua", O_RDONLY);
	if(fd < 0 && errno != ENOENT)
		return -1;
	if(fd >= 0) {
		strcpy(luafilename, "Tupfile.lua");
		return fd;
	}

	fd = openat(tf->cur_dfd, "Tupdefault.lua", O_RDONLY);
	if(fd < 0 && errno != ENOENT)
		return -1;
	if(fd >= 0) {
		strcpy(luafilename, "Tupdefault.lua");
		return fd;
	}
	while(tent->parent) {
		int x;

		n++;
		for(x=0; x<n; x++) {
			strcpy(luafilename + x*3, "../");
		}
		strcpy(luafilename + n*3, "Tupdefault.lua");
		fd = openat(tf->cur_dfd, luafilename, O_RDONLY);
		if(fd < 0 && errno != ENOENT)
			return -1;
		if(fd >= 0)
			return fd;
		tent = tent->parent;
	}
	errno = ENOENT;
	return -1;
}

static char *get_newline(char *p)
{
	char *newline;

	newline = strchr(p, '\n');
	if(!newline) {
		newline = strchr(p, '\0');
	}
	return newline;
}

static int parse_tupfile(struct tupfile *tf, struct buf *b, const char *filename)
{
	char *p, *e;
	char *line;
	int lno = 0;
	struct bin_head bl;
	struct if_stmt ifs;
	int rc = 0;
	char line_debug[128];

	LIST_INIT(&bl);
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
		newline = get_newline(p);
		if(!newline) {
			fprintf(tf->f, "tup error: Unable to find trailing nul-byte.\n");
			fprintf(tf->f, "  Line was: '%s'\n", line);
			return -1;
		}
		lno++;
		while(newline[-1] == '\\' || (newline[-2] == '\\' && newline[-1] == '\r')) {
			if (newline[-1] == '\r') {
				newline[-2] = ' ';
			}
			newline[-1] = ' ';
			newline[0] = ' ';
			newline = get_newline(p);
			if(!newline) {
				fprintf(tf->f, "tup error: Unable to find trailing nul-byte.\n");
				fprintf(tf->f, "  Line was: '%s'\n", line);
				return -1;
			}
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

		strncpy(line_debug, line, sizeof(line_debug));
		memcpy(line_debug + sizeof(line_debug) - 4, "...", 4);

		rc = 0;
		if(strcmp(line, "else") == 0) {
			rc = if_else(&ifs);
		} else if(strcmp(line, "endif") == 0) {
			rc = if_endif(&ifs);
		} else if(strncmp(line, "ifeq ", 5) == 0) {
			if(if_true(&ifs)) {
				rc = eval_eq(tf, line+5, newline);
			}
			if(rc >= 0)
				rc = if_add(&ifs, rc);
		} else if(strncmp(line, "ifneq ", 6) == 0) {
			if(if_true(&ifs)) {
				rc = eval_eq(tf, line+6, newline);
			}
			if(rc >= 0)
				rc = if_add(&ifs, !rc);
		} else if(strncmp(line, "ifdef ", 6) == 0) {
			if(if_true(&ifs)) {
				rc = var_ifdef(tf, line+6);
			}
			if(rc >= 0)
				rc = if_add(&ifs, rc);
		} else if(strncmp(line, "ifndef ", 7) == 0) {
			if(if_true(&ifs)) {
				rc = var_ifdef(tf, line+7);
			}
			if(rc >= 0)
				rc = if_add(&ifs, !rc);
		} else if(!if_true(&ifs)) {
			/* Skip the false part of an if block */
		} else if(strncmp(line, "error ", 6) == 0) {
			rc = error_directive(tf, line+6);
		} else if(strncmp(line, "include ", 8) == 0) {
			char *file;

			file = line + 8;
			file = eval(tf, file, ALLOW_NODES);
			if(!file) {
				rc = -1;
			} else {
				rc = include_file(tf, file);
				free(file);
			}
		} else if(strcmp(line, "include_rules") == 0) {
			rc = include_rules(tf);
		} else if(strncmp(line, "preload ", 8) == 0) {
			rc = preload(tf, line+8);
		} else if(strncmp(line, "run ", 4) == 0) {
			rc = run_script(tf, line+4, lno, &bl);
		} else if(strncmp(line, "export ", 7) == 0) {
			rc = export(tf, line+7);
		} else if(strcmp(line, ".gitignore") == 0) {
			tf->ign = 1;
		} else if(line[0] == ':') {
			rc = parse_rule(tf, line+1, lno, &bl);
		} else if(line[0] == '!') {
			rc = parse_bang_definition(tf, line, lno);
		} else if(line[0] == '*') {
			rc = parse_chain_definition(tf, line, lno);
		} else {
			rc = set_variable(tf, line);
		}

		if(rc == ERROR_DIRECTIVE_ERROR) {
			fprintf(tf->f, "tup error: Found 'error' command parsing %s line %i. Quitting.\n", filename, lno);
			return -1;
		}
		if(rc == SYNTAX_ERROR) {
			fprintf(tf->f, "tup error: Syntax error parsing %s line %i\n  Line was: '%s'\n", filename, lno, line_debug);
			return -1;
		}
		if(rc < 0) {
			fprintf(tf->f, "tup error: Error parsing %s line %i\n  Line was: '%s'\n", filename, lno, line_debug);
			return -1;
		}
	}
	if(if_check(&ifs) < 0) {
		fprintf(tf->f, "tup error: Error parsing %s: missing endif before EOF.\n", filename);
		return -1;
	}

	bin_list_del(&bl);

	return 0;
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

	lval = eval(tf, lval, DISALLOW_NODES);
	if(!lval)
		return -1;
	rval = eval(tf, rval, DISALLOW_NODES);
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
	tent = tup_db_get_var(tf->variant, var, strlen(var), NULL);
	if(!tent)
		return -1;
	if(tent->type == TUP_NODE_VAR) {
		rc = 1;
	} else {
		rc = 0;
	}
	if(tupid_tree_add_dup(&tf->input_root, tent->tnode.tupid) < 0)
		return -1;
	return rc;
}

static int error_directive(struct tupfile *tf, char *cmdline) {
	fprintf(tf->f, "Error:\n  ");
	if (strlen(cmdline)==0) {
		fprintf(tf->f, "Empty error directive\n");
	} else {
		fprintf(tf->f, "%s\n", cmdline);
	}
	return ERROR_DIRECTIVE_ERROR;
}

static int include_rules(struct tupfile *tf)
{
	char tuprules[] = "Tuprules.tup";
	int trlen = sizeof(tuprules) - 1;
	int rc = -1;
	struct stat buf;
	int num_dotdots;
	struct tup_entry *tent;
	char *path;
	char *p;
	int x;

	num_dotdots = 0;
	tent = tf->curtent;
	while(tent->tnode.tupid != tf->variant->dtnode.tupid) {
		tent = tent->parent;
		num_dotdots++;
	}
	path = malloc(num_dotdots * 3 + trlen + 1);
	if(!path) {
		parser_error(tf, "malloc");
		return -1;
	}

	p = path;
	for(x=0; x<num_dotdots; x++, p += 3) {
		strcpy(p, ".." PATH_SEP_STR);
	}
	strcpy(path + num_dotdots*3, tuprules);

	p = path;
	for(x=0; x<=num_dotdots; x++, p += 3) {
		if(fstatat(tf->cur_dfd, p, &buf, AT_SYMLINK_NOFOLLOW) == 0)
			if(include_file(tf, p) < 0)
				goto out_free;
	}
	rc = 0;

out_free:
	free(path);

	return rc;
}

struct readdir_parser_params {
	struct string_entries *root;
};

static int readdir_parser_cb(void *arg, struct tup_entry *tent)
{
	struct readdir_parser_params *rpp = arg;
	struct string_tree *st;

	/* Skip '$' */
	if(tent->tnode.tupid == env_dt())
		return 0;

	st = malloc(sizeof *st);
	if(string_tree_add(rpp->root, st, tent->name.s) < 0) {
		/* string_tree_add will fail if there is a dup. Just
		 * free our st and return.
		 */
		free(st);
	}
	return 0;
}

static void free_dir_list(struct string_entries *root, struct parser_directory *pd)
{
	struct string_tree *st;
	while(!RB_EMPTY(&pd->files)) {
		st = RB_MIN(string_entries, &pd->files);
		string_tree_free(&pd->files, st);
		free(st);
	}
	string_tree_free(root, &pd->st);
	free(pd);
}

static void free_dir_lists(struct string_entries *root)
{
	struct string_tree *st;

	while((st = RB_ROOT(root)) != NULL) {
		struct parser_directory *pd = container_of(st, struct parser_directory, st);

		free_dir_list(root, pd);
	}
}

static int gen_dir_list(struct tupfile *tf, tupid_t dt)
{
	char path[PATH_MAX];
	struct tup_entry *tent;
	struct tup_entry *srctent;
	struct parser_directory *pd;
	struct readdir_parser_params rpp;
	struct string_tree *st;

	if(tup_entry_add(dt, &tent) < 0)
		return -1;
	pd = malloc(sizeof *pd);
	if(!pd) {
		perror("malloc");
		return -1;
	}
	RB_INIT(&pd->files);

	rpp.root = &pd->files;

	if(snprint_tup_entry(path, sizeof(path), tent) >= (signed)sizeof(path)) {
		fprintf(tf->f, "tup internal error: ps.path is sized incorrectly in gen_dir_list()\n");
		return -1;
	}
	if(tup_db_select_node_dir_glob(readdir_parser_cb, &rpp, dt,
				       "*", -1, &tf->g->gen_delete_root, 1) < 0)
		return -EIO;
	if(variant_get_srctent(tf->variant, dt, &srctent) < 0)
		return -1;
	if(srctent) {
		if(tup_db_select_node_dir_glob(readdir_parser_cb, &rpp, srctent->tnode.tupid,
					       "*", -1, &tf->g->gen_delete_root, 1) < 0)
			return -EIO;
	}

	st = string_tree_search(&tf->ps->directories, path, strlen(path));
	if(st)
		free_dir_list(&tf->ps->directories, container_of(st, struct parser_directory, st));

	if(string_tree_add(&tf->ps->directories, &pd->st, path) < 0) {
		fprintf(tf->f, "tup internal error: Unable to add '%s' to the directories string tree\n", path);
		return -1;
	}
	return 0;
}

static int preload(struct tupfile *tf, char *cmdline)
{
	struct path_list_head plist;
	struct path_list *pl;
	char *eval_cmdline;

	eval_cmdline = eval(tf, cmdline, ALLOW_NODES);
	if(!eval_cmdline) {
		return -1;
	}

	TAILQ_INIT(&plist);
	if(get_path_list(tf, eval_cmdline, &plist, tf->curtent->tnode.tupid, NULL, 0) < 0)
		return -1;

	/* get_path_list() leaves us with the last path uncompleted (since it
	 * usually just handles filenames), so we resolve the last path here
	 * and store that in dt.  We don't need the pel for
	 * parse_dependent_tupfiles().
	 */
	TAILQ_FOREACH(pl, &plist, list) {
		struct tup_entry *tent;
		if(pl->pel->len == 2 && strncmp(pl->pel->path, "..", 2) == 0) {
			if(tup_entry_add(pl->dt, &tent) < 0)
				return -1;
			if(!tent->parent) {
				fprintf(tf->f, "tup error: Unable to preload a directory beyond the tup hierarchy.\n");
				return -1;
			}
			tent = tent->parent;
		} else {
			if(tup_db_select_tent_part(pl->dt, pl->pel->path, pl->pel->len, &tent) < 0)
				return -1;
			if(!tent) {
				fprintf(tf->f, "tup error: Unable to find node '%.*s' for preloading in directory %lli\n", pl->pel->len, pl->pel->path, pl->dt);
				tup_db_print(tf->f, pl->dt);
				return -1;
			}
		}
		if(tent->type != TUP_NODE_DIR) {
			fprintf(tf->f, "tup error: preload needs to specify a pathname, but node '%.*s' has type '%s'\n", pl->pel->len, pl->pel->path, tup_db_type(tent->type));
			return -1;
		}
		pl->dt = tent->tnode.tupid;
	}
	if(parse_dependent_tupfiles(&plist, tf) < 0)
		return -1;

	TAILQ_FOREACH(pl, &plist, list) {
		if(gen_dir_list(tf, pl->dt) < 0)
			return -1;
	}
	free_path_list(&plist);
	free(eval_cmdline);
	return 0;
}

static int run_script(struct tupfile *tf, char *cmdline, int lno,
		      struct bin_head *bl)
{
	int rc;
	char *eval_cmdline;

	eval_cmdline = eval(tf, cmdline, ALLOW_NODES);
	if(!eval_cmdline) {
		return -1;
	}
	rc = exec_run_script(tf, eval_cmdline, lno, bl);
	free(eval_cmdline);
	return rc;
}

int exec_run_script(struct tupfile *tf, const char *cmdline, int lno,
		    struct bin_head *bl)
{
	char *rules;
	char *p;
	int rslno = 0;
	int rc;
	struct tupid_tree *tt;

	pthread_mutex_lock(&tf->ps->lock);
	rc = gen_dir_list(tf, tf->tupid);
	pthread_mutex_unlock(&tf->ps->lock);
	if(rc < 0)
		return -1;

	if(debug_run)
		fprintf(tf->f, " --- run script output from '%s'\n", cmdline);

	/* Make sure we have a dependency on each environment variable, since
	 * these are passed to the run script.
	 */
	RB_FOREACH(tt, tupid_entries, &tf->env_root) {
		if(tupid_tree_add_dup(&tf->input_root, tt->tupid) < 0)
			return -1;
	}
	rc = server_run_script(tf->f, tf->tupid, cmdline, &tf->env_root, &rules);
	if(rc < 0)
		return -1;

	p = rules;
	while(p[0]) {
		char *newline;
		rslno++;
		if(p[0] != ':') {
			fprintf(tf->f, "tup error: run-script line %i is not a :-rule - '%s'\n", rslno, p);
			goto out_err;
		}
		newline = strchr(p, '\n');
		if(!newline) {
			fprintf(tf->f, "tup error: Missing newline from :-rule in run script: '%s'\n", p);
			goto out_err;
		}
		*newline = 0;
		if(debug_run)
			fprintf(tf->f, "%s\n", p);
		if(parse_rule(tf, p+1, lno, bl) < 0) {
			fprintf(tf->f, "tup error: Unable to parse :-rule from run script: '%s'\n", p);
			goto out_err;
		}
		p = newline + 1;
	}

	free(rules);
	return 0;

out_err:
	free(rules);
	return -1;
}

int export(struct tupfile *tf, const char *cmdline)
{
	struct tup_entry *tent = NULL;

	if(!cmdline[0]) {
		fprintf(tf->f, "tup error: Expected environment variable to export.\n");
		return SYNTAX_ERROR;
	}

	/* Pull from tup's environment */
	if(tup_db_findenv(cmdline, &tent) < 0) {
		fprintf(tf->f, "tup error: Unable to get tup entry for environment variable '%s'\n", cmdline);
		return -1;
	}
	if(tupid_tree_add_dup(&tf->env_root, tent->tnode.tupid) < 0)
		return -1;
	return 0;
}

static int gitignore(struct tupfile *tf, tupid_t dt)
{
	struct tup_entry *tent;

	if(tup_db_select_tent(dt, ".gitignore", &tent) < 0)
		return -1;
	if(!tent) {
		if(tf->refactoring) {
			fprintf(tf->f, "tup refactoring error: Attempting to create a new .gitignore file.\n");
			return -1;
		}
		if(tup_db_node_insert_tent(dt, ".gitignore", -1, TUP_NODE_GENERATED, -1, dt, &tent) < 0)
			return -1;
	} else {
		tree_entry_remove(&tf->g->gen_delete_root,
				  tent->tnode.tupid,
				  &tf->g->gen_delete_count);
		/* It may be a ghost if we are going from a variant
		 * to an in-tree build.
		 */
		if(tent->type == TUP_NODE_GHOST) {
			if(tup_db_set_type(tent, TUP_NODE_GENERATED) < 0)
				return -1;
		}
	}
	if(tupid_tree_add_dup(&tf->g->parse_gitignore_root, dt) < 0)
		return -1;
	return 0;
}

static int check_toplevel_gitignore(struct tupfile *tf)
{
	int fd;
	struct buf incb;
	struct tup_entry *tent = NULL;
	char *p;
	char *e;
	char *line;
	int rc;

	/* This mimics pieces of include_tuprules and parse_tupfile(), but all
	 * we're looking for is a .gitignore directive. We don't want to
	 * actually do include_rules(), since we'll potentially pick up real
	 * :-rules from Tuprules.tup, which would be confusing since there is
	 * no actual Tupfile. This isn't perfect, sicne it wouldn't handle a
	 * .gitignore directive inside a conditional, but that doesn't really
	 * make sense anyway.
	 */
	if(tup_db_select_tent(tf->tupid, "Tuprules.tup", &tent) < 0)
		return -1;
	if(!tent)
		return 0;
	fd = tup_entry_openat(tf->root_fd, tent);
	if(fd < 0) {
		parser_error(tf, "Tuprules.tup");
		return -1;
	}
	if(fslurp_null(fd, &incb) < 0)
		goto out_close;

	p = incb.s;
	e = incb.s + incb.len;
	while(p < e) {
		char *newline;

		/* Skip leading whitespace and empty lines */
		while(p < e && isspace(*p)) {
			p++;
		}
		/* If we just had empty lines at the end, we're done */
		if(p == e)
			break;

		line = p;
		newline = get_newline(p);
		if(!newline) {
			fprintf(tf->f, "tup error: Unable to find trailing nul-byte.\n");
			fprintf(tf->f, "  Line was: '%s'\n", line);
			goto out_free;
		}
		while(newline[-1] == '\\' || (newline[-2] == '\\' && newline[-1] == '\r')) {
			if (newline[-1] == '\r') {
				newline[-2] = ' ';
			}
			newline[-1] = ' ';
			newline[0] = ' ';
			newline = get_newline(p);
			if(!newline) {
				fprintf(tf->f, "tup error: Unable to find trailing nul-byte.\n");
				fprintf(tf->f, "  Line was: '%s'\n", line);
				goto out_free;
			}
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

		if(strcmp(line, ".gitignore") == 0) {
			tf->ign = 1;
			break;
		}
	}
	rc = 0;

out_free:
	free(incb.s);
out_close:
	if(close(fd) < 0) {
		parser_error(tf, "close(fd)");
		rc = -1;
	}
	return rc;
}

static int include_file(struct tupfile *tf, const char *file)
{
	struct buf incb;
	int fd;
	int rc = -1;
	struct pel_group pg;
	struct path_element *pel = NULL;
	struct tup_entry *tent = NULL;
	tupid_t newdt;
	struct tup_entry *oldtent = tf->curtent;
	int old_dfd = tf->cur_dfd;
	struct tup_entry *srctent = NULL;
	struct tup_entry *newtent;
	char *lua;

	if(get_path_elements(file, &pg) < 0)
		goto out_err;
	if(pg.pg_flags & PG_HIDDEN) {
		fprintf(tf->f, "tup error: Unable to include file with hidden path element.\n");
		goto out_del_pg;
	}
	newdt = find_dir_tupid_dt_pg(tf->f, tf->curtent->tnode.tupid, &pg, &pel, 0, 0);
	if(newdt <= 0) {
		fprintf(tf->f, "tup error: Unable to find directory for include file '%s' relative to '", file);
		print_tup_entry(tf->f, tf->curtent);
		fprintf(tf->f, "'\n");
		goto out_del_pg;
	}
	if(!pel) {
		fprintf(tf->f, "tup error: Invalid include filename: '%s'\n", file);
		goto out_del_pg;
	}

	newtent = tup_entry_get(newdt);
	if(tup_entry_variant(newtent) != tf->variant) {
		fprintf(tf->f, "tup error: Unable to include file '%s' since it is outside of the variant tree.\n", file);
		return -1;
	}
	tf->curtent = newtent;

	if(variant_get_srctent(tf->variant, newdt, &srctent) < 0)
		return -1;
	if(!srctent)
		srctent = tf->curtent;
	if(tup_db_select_tent_part(srctent->tnode.tupid, pel->path, pel->len, &tent) < 0 || !tent) {
		fprintf(tf->f, "tup error: Unable to find tup entry for file '%s'\n", file);
		goto out_free_pel;
	}

	tf->cur_dfd = tup_entry_openat(tf->root_fd, tent->parent);
	if (tf->cur_dfd < 0) {
		parser_error(tf, file);
		goto out_free_pel;
	}
	fd = tup_entry_openat(tf->root_fd, tent);
	if(fd < 0) {
		parser_error(tf, file);
		goto out_close_dfd;
	}
	if(fslurp_null(fd, &incb) < 0)
		goto out_close;

	lua = strstr(file, ".lua");
	/* strcmp is to make sure .lua is at the end of the filename */
	if(lua && strcmp(lua, ".lua") == 0) {
		if(parse_lua_tupfile(tf, &incb, file) < 0)
			goto out_free;
	} else {
		if(parse_tupfile(tf, &incb, file) < 0)
			goto out_free;
	}
	rc = 0;
out_free:
	free(incb.s);
out_close:
	if(close(fd) < 0) {
		parser_error(tf, "close(fd)");
		rc = -1;
	}
out_close_dfd:
	if(close(tf->cur_dfd) < 0) {
		parser_error(tf, "close(tf->cur_dfd)");
		rc = -1;
	}
out_free_pel:
	free(pel);
out_del_pg:
	del_pel_group(&pg);

out_err:
	tf->curtent = oldtent;
	tf->cur_dfd = old_dfd;
	if(rc < 0) {
		fprintf(tf->f, "tup error: Failed to parse included file '%s'\n", file);
		return -1;
	}

	return 0;
}

static int parse_rule(struct tupfile *tf, char *p, int lno, struct bin_head *bl)
{
	char *input, *cmd, *output, *bin;
	int cmd_len;
	struct rule r;
	int rc;
	int swapio;
	char *separator;

	if(split_input_pattern(tf, p, &input, &cmd, &cmd_len, &output, &bin, &swapio) < 0)
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
	}

	/* Make sure we have an input string, and the input string doesn't
	 * just contain order-only inputs.
	 */
	if(input && input[0] != '|') {
		r.empty_input = 0;
	} else {
		r.empty_input = 1;
	}
	r.input_pattern = input;

	r.output_pattern = output;
	separator = strchr(output, '|');
	if(separator) {
		r.extra_outputs = separator + 1;
		while(isspace(*r.extra_outputs))
			r.extra_outputs++;
		*separator = 0;
	} else {
		r.extra_outputs = NULL;
	}

	r.command = cmd;
	r.command_len = cmd_len;
	r.extra_command = NULL;
	r.line_number = lno;
	r.output_nl = NULL;

	if(r.command[0] == '!') {
		char *space;
		space = memchr(r.command, ' ', r.command_len);
		if(space) {
			*space = 0;
			r.extra_command = space + 1;
			r.command_len = strlen(r.command);
		}
	}

	if(swapio)
		rc = execute_reverse_rule(tf, &r, bl);
	else
		rc = execute_rule(tf, &r, bl);
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
		fprintf(tf->f, "tup error: Parse error line %i: Expecting '=' to set the bang rule.\n", lno);
		return SYNTAX_ERROR;
	}

	if(value[0] == '!') {
		/* Alias one macro as another */
		struct bang_rule *cur_br;

		st = string_tree_search(&tf->bang_root, p, strlen(p));
		if(st) {
			free_bang_rule(&tf->bang_root,
				       container_of(st, struct bang_rule, st));
		}

		st = string_tree_search(&tf->bang_root, value, strlen(value));
		if(!st) {
			fprintf(tf->f, "tup error: Unable to find !-macro '%s'\n", value);
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
				parser_error(tf, "strdup");
				goto err_cleanup_br;
			}
		} else {
			br->input = NULL;
		}
		br->command = strdup(cur_br->command);
		if(!br->command) {
			parser_error(tf, "strdup");
			goto err_cleanup_br;
		}
		br->output_pattern = strdup(cur_br->output_pattern);
		if(!br->output_pattern) {
			parser_error(tf, "strdup");
			goto err_cleanup_br;
		}
		if(cur_br->extra_outputs) {
			br->extra_outputs = strdup(cur_br->extra_outputs);
			if(!br->extra_outputs) {
				parser_error(tf, "strdup");
				goto err_cleanup_br;
			}
		} else {
			br->extra_outputs = NULL;
		}

		br->command_len = cur_br->command_len;

		if(string_tree_add(&tf->bang_root, &br->st, p) < 0) {
			fprintf(tf->f, "tup internal error: Error inserting bang rule into tree\n");
			goto err_cleanup_br;
		}
		return 0;
	}

	alloc_value = strdup(value);
	if(!alloc_value) {
		parser_error(tf, "strdup");
		return -1;
	}

	if(split_input_pattern(tf, alloc_value, &input, &command, &command_len,
			       &output, &bin, &swapio) < 0)
		return -1;
	if(bin != NULL) {
		fprintf(tf->f, "tup error: bins aren't allowed in !-macros. Rule was: %s = %s\n", p, alloc_value);
		return -1;
	}
	if(swapio) {
		fprintf(tf->f, "tup error: !-macro must use '|>' separators\n");
		return SYNTAX_ERROR;
	}

	if(input) {
		if(strncmp(input, "foreach", 7) == 0) {
			foreach = 1;
			input += 7;
			while(*input == ' ') input++;
		}
	}

	st = string_tree_search(&tf->bang_root, p, strlen(p));
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

		if(string_tree_add(&tf->bang_root, &br->st, p) < 0) {
			fprintf(tf->f, "tup internal error: Error inserting bang rule into tree\n");
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
	struct bang_list_head *destlist;

	value = split_eq(p);
	if(!value) {
		fprintf(tf->f, "tup error: Parse error line %i: Expecting '=' to set the *-chain.\n", lno);
		return -1;
	}

	lbracket = strchr(p, '[');
	if(lbracket) {
		char *rbracket;
		*lbracket = 0;
		input_pattern = lbracket+1;
		rbracket = strchr(input_pattern, ']');
		if(!rbracket) {
			fprintf(tf->f, "tup error: Parse error line %i: Expecting ']' character in *-chain definition\n", lno);
			return -1;
		}
		*rbracket = 0;
	}
	st = string_tree_search(&tf->chain_root, p, strlen(p));
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
			parser_error(tf, "malloc");
			return -1;
		}
		TAILQ_INIT(&ch->src_chain_list);
		TAILQ_INIT(&ch->banglist);

		if(string_tree_add(&tf->chain_root, &ch->st, p) < 0) {
			fprintf(tf->f, "tup internal error: Error inserting *-chain into tree\n");
			return -1;
		}
	}

	destlist = &ch->banglist;
	if(input_pattern) {
		struct src_chain *sc;
		sc = malloc(sizeof *sc);
		if(!sc) {
			parser_error(tf, "malloc");
			return -1;
		}
		sc->input_pattern = strdup(input_pattern);
		if(!sc->input_pattern) {
			parser_error(tf, "strdup");
			free(sc);
			return -1;
		}
		TAILQ_INIT(&sc->banglist);
		TAILQ_INSERT_TAIL(&ch->src_chain_list, sc, list);
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
			fprintf(tf->f, "tup error: *-chain must be composed of !-macros, not '%s'\n", value);
			return -1;
		}
		bst = string_tree_search(&tf->bang_root, value, strlen(value));
		if(!bst) {
			fprintf(tf->f, "tup error: Unable to find !-macro: '%s'\n", value);
			return -1;
		}
		br = container_of(bst, struct bang_rule, st);

		bal = malloc(sizeof *bal);
		if(!bal) {
			parser_error(tf, "malloc");
			return -1;
		}
		bal->br = br;
		TAILQ_INSERT_TAIL(destlist, bal, list);

		value = p;
	} while(p != NULL);

	return 0;
}

static int set_variable(struct tupfile *tf, char *line)
{
	char *eq;
	char *var;
	char *value;
	int append;
	int rc = 0;

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
				return SYNTAX_ERROR;
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

	var = eval(tf, line, DISALLOW_NODES);
	if(!var)
		return -1;
	value = eval(tf, value, DISALLOW_NODES);
	if(!value)
		return -1;

	if(strncmp(var, "CONFIG_", 7) == 0) {
		fprintf(tf->f, "tup error: Unable to override setting of variable '%s' because it begins with 'CONFIG_'. These variables can only be set in the tup.config file.\n", var);
		return -1;
	}

	if(var[0] == '&') {
		struct tup_entry *tent;
		tent = get_tent_dt(tf->curtent->tnode.tupid, value);
		if(!tent || tent->type == TUP_NODE_GHOST) {
			/* didn't find the given file; if using a variant, check the source dir */
			struct tup_entry *srctent;
			if(variant_get_srctent(tf->variant, tf->curtent->tnode.tupid, &srctent) < 0)
				return -1;
			if(srctent)
				tent = get_tent_dt(srctent->tnode.tupid, value);

			if(!tent) {
				fprintf(tf->f, "tup error: Unable to find tup entry for file '%s' in node reference declaration.\n", value);
				return -1;
			}
		}

		if(tent->type != TUP_NODE_FILE && tent->type != TUP_NODE_DIR) {
			fprintf(tf->f, "tup error: Node-variables can only refer to normal files and directories, not a '%s'.\n", tup_db_type(tent->type));
			return -1;
		}

		/* var+1 to skip the leading '&' */
		if(append)
			rc = nodedb_append(&tf->node_db, var+1, tent);
		else
			rc = nodedb_set(&tf->node_db, var+1, tent);
	} else {
		if(append)
			rc = vardb_append(&tf->vdb, var, value);
		else
			rc = vardb_set(&tf->vdb, var, value, NULL);
	}
	if(rc < 0) {
		fprintf(tf->f, "tup internal error: Error setting variable '%s'\n", var);
		return -1;
	}
	free(var);
	free(value);
	return 0;
}

static int parse_bang_rule_internal(struct tupfile *tf, struct rule *r,
				    struct string_tree *st, struct name_list *nl)
{
	struct bang_rule *br;
	char *tinput;
	br = container_of(st, struct bang_rule, st);

	/* Add any order only inputs to the list */
	if(nl && br->input) {
		tinput = tup_printf(tf, br->input, -1, nl, NULL, NULL, 0, NULL);
		if(!tinput)
			return -1;
	} else {
		tinput = br->input;
	}
	if(parse_input_pattern(tf, tinput, NULL, &r->bang_oo_inputs, NULL, 1) < 0)
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

static int parse_empty_bang_rule(struct tupfile *tf, struct rule *r)
{
	struct string_tree *st;
	char empty[] = ".EMPTY";
	char tmp[r->command_len + sizeof(empty)];

	memcpy(tmp, r->command, r->command_len);
	memcpy(tmp + r->command_len, empty, sizeof(empty));

	st = string_tree_search(&tf->bang_root, tmp, sizeof(tmp) - 1);
	if(!st)
		return 1;
	return parse_bang_rule_internal(tf, r, st, NULL);
}

static int parse_bang_rule(struct tupfile *tf, struct rule *r,
			   struct name_list *nl, const char *ext, int extlen)
{
	struct string_tree *st;
	char tmp[r->command_len + extlen + 1];

	memcpy(tmp, r->command, r->command_len);
	if(ext)
		memcpy(tmp + r->command_len, ext, extlen + 1);

	/* First try to find the extension-specific rule, and if not then use
	 * the general one. Eg: if the input is foo.c, then the extension is ".c",
	 * so try "!cc.c" first, then "!cc" second.
	 */
	st = string_tree_search(&tf->bang_root, tmp, sizeof(tmp) - 1);
	if(!st) {
		st = string_tree_search(&tf->bang_root, r->command, r->command_len);
		if(!st) {
			fprintf(tf->f, "tup error: Error finding bang variable: '%s'\n",
				r->command);
			return -1;
		}
	}
	return parse_bang_rule_internal(tf, r, st, nl);
}

static void free_bang_rule(struct string_entries *root, struct bang_rule *br)
{
	string_tree_free(root, &br->st);

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

static void free_bang_tree(struct string_entries *root)
{
	struct string_tree *st;

	while((st = RB_ROOT(root)) != NULL) {
		free_bang_rule(root, container_of(st, struct bang_rule, st));
	}
}

static void free_chain_tree(struct string_entries *root)
{
	struct string_tree *st;

	while((st = RB_ROOT(root)) != NULL) {
		struct chain *ch = container_of(st, struct chain, st);
		struct src_chain *sc;

		string_tree_free(root, st);

		while(!TAILQ_EMPTY(&ch->src_chain_list)) {
			sc = TAILQ_FIRST(&ch->src_chain_list);
			TAILQ_REMOVE(&ch->src_chain_list, sc, list);
			free_banglist(&sc->banglist);
			free(sc->input_pattern);
			free(sc);
		}

		free_banglist(&ch->banglist);
		free(ch);
	}
}

static void free_banglist(struct bang_list_head *head)
{
	while(!TAILQ_EMPTY(head)) {
		struct bang_list *bal = TAILQ_FIRST(head);
		TAILQ_REMOVE(head, bal, list);
		free(bal);
	}
}

static int split_input_pattern(struct tupfile *tf, char *p, char **o_input,
			       char **o_cmd, int *o_cmdlen, char **o_output,
			       char **o_bin, int *swapio)
{
	char *input;
	char *cmd;
	char *output;
	char *bin = NULL;
	char *brace;
	char *ebrace = NULL;
	char *ie, *ce;
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
		if(!p) {
			fprintf(tf->f, "tup error: Missing '|>' marker.\n");
			return -1;
		}
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
	if(!p) {
		fprintf(tf->f, "tup error: Missing second '%s' marker.\n", marker);
		return -1;
	}
	ce = p - 1;
	while(isspace(*ce) && ce > cmd)
		ce--;
	p += 2;
	output = p;
	while(isspace(*output))
		output++;
	ce[1] = 0;

	brace = output;
	while(1) {
		brace = strchr(brace, '{');
		if(!brace)
			break;
		ebrace = brace - 1;

		/* If the character before the '{' is not a space (and we're
		 * not the first character in the output string), then it's
		 * part of a filename.
		 */
		if(brace != output && !isspace(*ebrace)) {
			brace++;
			continue;
		}
		while(isspace(*ebrace) && ebrace > output)
			ebrace--;
		bin = brace + 1;

		brace = strchr(bin, '}');
		if(!brace) {
			fprintf(tf->f, "tup error: Missing '}' to finish bin.\n");
			return -1;
		}
		*brace = 0;
		ebrace[1] = 0;
		if(brace[1]) {
			fprintf(tf->f, "tup error: bin must be at the end of the output list.\n");
			return -1;
		}
		break;
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
			       struct bin_head *bl, int required)
{
	char *eval_pattern;
	char *oosep;

	if(!input_pattern)
		return 0;

	eval_pattern = eval(tf, input_pattern, ALLOW_NODES);
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
		if(input_pattern_to_nl(tf, oosep, order_only_inputs, bl, required) < 0)
			return -1;
	}
	if(inputs) {
		if(input_pattern_to_nl(tf, eval_pattern, inputs, bl, required) < 0)
			return -1;
	} else {
		if(eval_pattern[0]) {
			fprintf(tf->f, "tup error: bang rules can't have normal inputs, only order-only inputs. Pattern was: %s\n", input_pattern);
			return -1;
		}
	}
	free(eval_pattern);
	return 0;
}

static int execute_rule(struct tupfile *tf, struct rule *r, struct bin_head *bl)
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
			       &r->order_only_inputs, bl, 1) < 0)
		return -1;

	make_name_list_unique(&r->inputs);

	init_name_list(&output_nl);

	if(r->command[0] == '*') {
		struct string_tree *st;
		struct chain *ch;
		struct bang_list *bal;
		struct bang_list_head *banglist;

		last_output_pattern = r->output_pattern;
		r->output_pattern = empty_pattern;

		st = string_tree_search(&tf->chain_root, r->command, r->command_len);
		if(!st) {
			fprintf(tf->f, "tup error: Unable to find *-chain: '%s'\n", r->command);
			return -1;
		}
		ch = container_of(st, struct chain, st);

		banglist = &ch->banglist;
		if(!r->inputs.num_entries && r->output_nl) {
			struct src_chain *sc;
			const char *ext = NULL;

			if(r->output_nl->num_entries > 0) {
				nle = TAILQ_FIRST(&r->output_nl->entries);
				if(nle->base &&
				   nle->extlessbaselen != nle->baselen) {
					ext = nle->base + nle->extlessbaselen;
				}
			}
			TAILQ_FOREACH(sc, &ch->src_chain_list, list) {
				char *tinput;
				char *input_pattern;

				banglist = &sc->banglist;

				tinput = tup_printf(tf, sc->input_pattern, -1, r->output_nl, NULL, NULL, 0, NULL);
				if(!tinput)
					return -1;
				input_pattern = eval(tf, tinput, DISALLOW_NODES);
				free(tinput);
				if(!input_pattern)
					return -1;
				if(check_recursive_chain(tf, input_pattern, bl, r, ext) < 0)
					return -1;
				delete_name_list(&r->order_only_inputs);
				if(parse_input_pattern(tf, input_pattern, &r->inputs,
						       &r->order_only_inputs, bl, 0) < 0)
					return -1;
				make_name_list_unique(&r->inputs);
				free(input_pattern);
				if(r->inputs.num_entries)
					break;
			}
			if(!r->inputs.num_entries) {
				fprintf(tf->f, "tup error: Unable to find any inputs for the *-chain for output '%s' in rule at line %i\n", last_output_pattern, r->line_number);
				return -1;
			}
		}

		TAILQ_FOREACH(bal, banglist, list) {
			if(bal == TAILQ_LAST(banglist, bang_list_head)) {
				/* Apply the output pattern specified in the rule
				 * to the last !-macro in the *-chain.
				 */
				r->output_pattern = last_output_pattern;
			} else {
				if(strcmp(bal->br->output_pattern, "") == 0) {
					fprintf(tf->f, "tup error: Intermediate !-macro '%s' in *-chain '%s' does not specify an output pattern.\n", bal->br->st.s, ch->st.s);
					return -1;
				}
			}
			r->command = bal->br->st.s;
			r->command_len = bal->br->st.len;

			if(execute_rule_internal(tf, r, &output_nl) < 0)
				return -1;

			delete_name_list(&r->inputs);
			move_name_list(&r->inputs, &output_nl);
		}
		delete_name_list(&r->inputs);
	} else {
		if(execute_rule_internal(tf, r, &output_nl) < 0)
			return -1;
		delete_name_list(&output_nl);
	}

	return 0;
}

static int execute_rule_internal(struct tupfile *tf, struct rule *r,
				 struct name_list *output_nl)
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
		st = string_tree_search(&tf->bang_root, r->command, r->command_len);
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
		const char *old_command = NULL;
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
		while(!TAILQ_EMPTY(&r->inputs.entries)) {
			const char *ext = NULL;
			int extlen = 0;

			nle = TAILQ_FIRST(&r->inputs.entries);

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
				if(parse_bang_rule(tf, r, &tmp_nl, ext, ext ? strlen(ext) : 0) < 0)
					return -1;
			}
			/* The extension in do_rule() does not include the
			 * leading '.'
			 */
			if(do_rule_output_pattern(tf, r, &tmp_nl, ext+1, extlen-1, output_nl) < 0)
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
		 * empty, we won't try to execute do_rule_output_pattern(). But
		 * an empty user string implies that no input is required.
		 */
		if((r->inputs.num_entries > 0 || r->empty_input)) {
			if(is_bang) {
				if(parse_bang_rule(tf, r, NULL, NULL, 0) < 0)
					return -1;
			}

			if(do_rule_output_pattern(tf, r, &r->inputs, NULL, 0, output_nl) < 0)
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

				rc = parse_empty_bang_rule(tf, r);
				if(rc < 0)
					return -1;
				if(rc == 0) {
					if(do_rule_output_pattern(tf, r, &r->inputs,
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
				struct bin_head *bl)
{
	struct path_list_head oplist;
	struct path_list *pl;
	char *eval_pattern;
	struct name_list tmp_nl;
	struct name_list_entry tmp_nle;

	if(!r->foreach) {
		fprintf(tf->f, "tup error: reverse rule must use 'foreach'\n");
		return -1;
	}
	if(!r->input_pattern) {
		fprintf(tf->f, "tup error: reverse rule must have input list\n");
		return -1;
	}
	eval_pattern = eval(tf, r->input_pattern, DISALLOW_NODES);
	if(!eval_pattern)
		return -1;

	TAILQ_INIT(&oplist);
	if(get_path_list(tf, eval_pattern, &oplist, tf->tupid, NULL, 0) < 0)
		return -1;
	make_path_list_unique(&oplist);

	while(!TAILQ_EMPTY(&oplist)) {
		struct rule tmpr;
		char *tinput;
		char *input_pattern;

		pl = TAILQ_FIRST(&oplist);

		if(pl->path) {
			/* Things with paths (eg: foo/built-in.o) get skipped.
			 * This is pretty much hacked to get linux to work.
			 */
			goto out_skip;
		}

		init_name_list(&tmp_nl);
		tmp_nle.path = malloc(pl->pel->len + 1);
		if(!tmp_nle.path) {
			parser_error(tf, "malloc");
			return -1;
		}
		memcpy(tmp_nle.path, pl->pel->path, pl->pel->len);
		tmp_nle.path[pl->pel->len] = 0;
		tmp_nle.len = pl->pel->len;
		tmp_nle.extlesslen = tmp_nle.len - 1;
		while(tmp_nle.extlesslen > 0 && tmp_nle.path[tmp_nle.extlesslen] != '.')
			tmp_nle.extlesslen--;

		tmp_nle.tent = tup_db_create_node_part(tf->tupid, tmp_nle.path, -1,
						       TUP_NODE_GENERATED, -1, NULL);
		if(!tmp_nle.tent)
			return -1;
		set_nle_base(&tmp_nle);
		add_name_list_entry(&tmp_nl, &tmp_nle);

		tinput = tup_printf(tf, r->output_pattern, -1, &tmp_nl, NULL, NULL, 0, NULL);
		if(!tinput)
			return -1;
		input_pattern = eval(tf, tinput, DISALLOW_NODES);
		free(tinput);
		if(!input_pattern)
			return -1;

		tmpr.foreach = 0;
		tmpr.input_pattern = input_pattern;
		tmpr.output_pattern = tmp_nle.path;
		tmpr.extra_outputs = NULL;
		tmpr.bin = r->bin;
		tmpr.command = r->command;
		tmpr.extra_command = NULL;
		tmpr.command_len = r->command_len;
		tmpr.empty_input = 0;
		tmpr.line_number = r->line_number;
		tmpr.output_nl = &tmp_nl;

		if(execute_rule(tf, &tmpr, bl) < 0)
			return -1;
		free(input_pattern);
		free(tmp_nle.path);

out_skip:
		del_pl(pl, &oplist);
	}
	free(eval_pattern);

	return 0;
}

static int check_recursive_chain(struct tupfile *tf, const char *input_pattern,
				 struct bin_head *bl,
				 struct rule *r, const char *ext)
{
	struct path_list_head inp_list;
	char *inp;
	struct path_list *pl;
	int extlen = ext ? strlen(ext) : 0;

	inp = strdup(input_pattern);
	if(!inp) {
		parser_error(tf, "strdup");
		return -1;
	}

	TAILQ_INIT(&inp_list);
	if(get_path_list(tf, inp, &inp_list, tf->tupid, NULL, 0) < 0)
		return -1;
	make_path_list_unique(&inp_list);

	while(!TAILQ_EMPTY(&inp_list)) {
		pl = TAILQ_FIRST(&inp_list);

		if(pl->pel->len > extlen) {
			if(name_cmp(pl->pel->path + pl->pel->len - extlen, ext) == 0) {
				struct rule tmpr;
				char output_pattern[] = "";
				char *tinput;

				tinput = malloc(pl->pel->len + 1);
				if(!tinput) {
					parser_error(tf, "malloc");
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
				tmpr.extra_command = NULL;
				tmpr.empty_input = 0;
				tmpr.line_number = r->line_number;
				tmpr.output_nl = NULL;
				if(execute_reverse_rule(tf, &tmpr, bl) < 0)
					return -1;
				free(tinput);
			}
		}

		del_pl(pl, &inp_list);
	}
	free(inp);
	return 0;
}

static int input_pattern_to_nl(struct tupfile *tf, char *p,
			       struct name_list *nl, struct bin_head *bl,
			       int required)
{
	struct path_list_head plist;

	TAILQ_INIT(&plist);
	if(get_path_list(tf, p, &plist, tf->tupid, bl, 0) < 0)
		return -1;
	if(parse_dependent_tupfiles(&plist, tf) < 0)
		return -1;
	if(get_name_list(tf, &plist, nl, required) < 0)
		return -1;
	free_path_list(&plist);
	return 0;
}

struct path_list *new_pl(struct tupfile *tf, char *mem)
{
	struct path_list *pl;

	pl = malloc(sizeof *pl);
	if(!pl) {
		parser_error(tf, "malloc");
		return NULL;
	}
	pl->path = NULL;
	pl->pel = NULL;
	pl->mem = mem;
	pl->group = 0;
	pl->dt = 0;
	pl->bin = NULL;
	return pl;
}

static int get_path_list(struct tupfile *tf, char *p, struct path_list_head *plist,
			 tupid_t dt, struct bin_head *bl, int create_output_dirs)
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

		pl = new_pl(tf, NULL);
		if(!pl) {
			return -1;
		}

		if(p[0] == '{') {
			/* Bin */
			char *endb;

			if(!bl) {
				fprintf(tf->f, "tup error: Bins are only usable in an input or output list.\n");
				return -1;
			}

			endb = strchr(p, '}');
			if(!endb) {
				fprintf(tf->f, "tup error: Expecting end bracket for input bin.\n");
				return -1;
			}
			*endb = 0;
			pl->bin = bin_find(p+1, bl);
			if(!pl->bin) {
				fprintf(tf->f, "tup error: Unable to find bin '%s'\n", p+1);
				return -1;
			}
		} else {
			/* Path */
			if(get_pl(tf, p, pl, dt, create_output_dirs) < 0)
				return -1;
		}
		TAILQ_INSERT_TAIL(plist, pl, list);

skip_empty_space:
		p += spc_index + 1;
	} while(!last_entry);

	return 0;
}

int get_pl(struct tupfile *tf, char *p, struct path_list *pl,
	   tupid_t dt, int create_output_dirs)
{
	struct pel_group pg;
	int sotgv = 0;

	if(strchr(p, '<') != NULL) {
		/* Group */
		char *endb;
		endb = strchr(p, '>');
		if(!endb) {
			fprintf(tf->f, "tup error: Expecting end angle bracket '>' character for group.\n");
			return -1;
		}
		pl->group = 1;
	}
	pl->path = p;

	if(get_path_elements(p, &pg) < 0)
		return -1;
	if(pg.pg_flags & PG_HIDDEN) {
		fprintf(tf->f, "tup error: You specified a path '%s' that contains a hidden filename (since it begins with a '.' character). Tup ignores these files - please remove references to it from the Tupfile.\n", p);
		return -1;
	}

	if(create_output_dirs || pg.pg_flags & PG_GROUP)
		sotgv = SOTGV_CREATE_DIRS;
	pl->dt = find_dir_tupid_dt_pg(tf->f, dt, &pg, &pl->pel, sotgv, 0);
	if(pl->dt <= 0) {
		fprintf(tf->f, "tup error: Failed to find directory ID for dir '%s' relative to %lli\n", p, dt);
		return -1;
	}
	if(!pl->pel) {
		if(strcmp(pl->path, ".") == 0) {
			fprintf(tf->f, "tup error: Not expecting '.' path here.\n");
			return -1;
		}
		fprintf(tf->f, "tup internal error: Final pel missing for path: '%s'\n", pl->path);
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
	return 0;
}

static void make_path_list_unique(struct path_list_head *plist)
{
	/* When make_name_list_unique can't be used, this can make a path_list
	 * unique in O(n^2) time.
	 */
	struct path_list *pl;
	struct path_list *tmp;

	TAILQ_FOREACH_SAFE(pl, plist, list, tmp) {
		struct path_list *pl2;

		pl2 = TAILQ_NEXT(pl, list);
		while(pl2 != NULL) {
			if(pl->pel->len == pl2->pel->len &&
			   memcmp(pl->pel->path, pl2->pel->path, pl->pel->len) == 0) {
				del_pl(pl, plist);
				break;
			}
			pl2 = TAILQ_NEXT(pl2, list);
		}
	}
}

void free_path_list(struct path_list_head *plist)
{
	struct path_list *pl, *tmp;

	TAILQ_FOREACH_SAFE(pl, plist, list, tmp) {
		del_pl(pl, plist);
	}
}

void del_pl(struct path_list *pl, struct path_list_head *head)
{
	TAILQ_REMOVE(head, pl, list);
	free(pl->mem);
	free(pl->pel);
	free(pl);
}

static void make_name_list_unique(struct name_list *nl)
{
	struct name_list_entry *tmp;
	struct tup_entry_head *input_list;
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
	TAILQ_FOREACH_SAFE(nle, &nl->entries, list, tmp) {
		if(tup_entry_in_list(nle->tent)) {
			delete_name_list_entry(nl, nle);
		} else {
			tup_entry_list_add(nle->tent, input_list);
		}
	}
	tup_entry_release_list();
}

int parse_dependent_tupfiles(struct path_list_head *plist, struct tupfile *tf)
{
	struct path_list *pl;

	TAILQ_FOREACH(pl, plist, list) {
		/* Only care about non-bins, and directories that are not our
		 * own.
		 */
		if(!pl->bin && pl->dt != tf->tupid && !pl->group) {
			struct node *n;
			struct tup_entry *dtent;

			if(tup_entry_add(pl->dt, &dtent) < 0)
				return -1;
			if(dtent->type == TUP_NODE_GENERATED_DIR) {
				if(tupid_tree_search(&tf->directory_root, pl->dt) == NULL) {
					fprintf(tf->f, "tup error: Unable to use inputs from a generated directory (%lli) that isn't written to by this Tupfile.\n", pl->dt);
					tup_db_print(tf->f, pl->dt);
					return -1;
				}
			}
			n = find_node(tf->g, pl->dt);
			if(n != NULL && !n->already_used) {
				int rc;
				struct timespan ts;
				n->already_used = 1;
				rc = parse(n, tf->g, &ts, tf->refactoring, tf->use_server);
				if(rc < 0) {
					if(rc == CIRCULAR_DEPENDENCY_ERROR) {
						fprintf(tf->f, "tup error: Unable to parse dependent Tupfile due to circular directory-level dependencies: ");
						tf->circular_dep_error = 1;
					} else {
						fprintf(tf->f, "tup error: Unable to parse dependent Tupfile: ");
					}
					print_tup_entry(tf->f, n->tent);
					fprintf(tf->f, "\n");
					return -1;
				}
				/* Ignore any time the dependent Tupfile was parsing, so that
				 * we don't account for it twice.
				 */
				timespan_add_delta(&tf->ts, &ts);
			}
			if(tupid_tree_add_dup(&tf->input_root, pl->dt) < 0)
				return -1;
		}
	}
	return 0;
}

int get_name_list(struct tupfile *tf, struct path_list_head *plist,
		  struct name_list *nl, int required)
{
	struct path_list *pl;

	TAILQ_FOREACH(pl, plist, list) {
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
	/* Save the original string with globs to pass to the function that
	 * determines the length, extension length, and basename length in order to
	 * handle globs.
	 */
	args.globstr = pl->pel->path;
	args.globstrlen = pl->pel->len;
	args.tf = tf;
	if(char_find(pl->pel->path, pl->pel->len, "*?[") == 0) {
		struct tup_entry *tent;
		struct variant *variant;

		if(tup_db_select_tent_part(pl->dt, pl->pel->path, pl->pel->len, &tent) < 0) {
			return -1;
		}
		if(!tent || tent->type == TUP_NODE_GHOST) {
			if(pl->pel->path[0] == '<') {
				tent = tup_db_create_node_part(pl->dt, pl->pel->path, pl->pel->len, TUP_NODE_GROUP, -1, NULL);
				if(!tent) {
					fprintf(tf->f, "tup error: Unable to create node for group: '%.*s'\n", pl->pel->len, pl->pel->path);
					return -1;
				}
			} else {
				struct tup_entry *srctent = NULL;

				if(variant_get_srctent(tf->variant, pl->dt, &srctent) < 0)
					return -1;
				if(srctent)
					if(tup_db_select_tent_part(srctent->tnode.tupid, pl->pel->path, pl->pel->len, &tent) < 0)
						return -1;
			}
		}

		if(!tent) {
			if(!required)
				return 0;
			fprintf(tf->f, "tup error: Explicitly named file '%.*s' not found in subdir '", pl->pel->len, pl->pel->path);
			print_tupid(tf->f, pl->dt);
			fprintf(tf->f, "'\n");
			return -1;
		}
		variant = tup_entry_variant(tent);
		if(!variant->root_variant && variant != tf->variant) {
			fprintf(tf->f, "tup error: Unable to use files from another variant (%s) in this variant (%s)\n", variant->variant_dir, tf->variant->variant_dir);
			return -1;
		}
		if(tent->type == TUP_NODE_GHOST) {
			if(!required)
				return 0;
			fprintf(tf->f, "tup error: Explicitly named file '%.*s' is a ghost file, so it can't be used as an input.\n", pl->pel->len, pl->pel->path);
			return -1;
		}
		if(tupid_tree_search(&tf->g->gen_delete_root, tent->tnode.tupid) != NULL) {
			struct tup_entry *srctent = NULL;
			int valid_input = 0;

			if(!required)
				return 0;

			/* If the file now exists in the srctree (ie: we
			 * deleted the rule to create a generated file and
			 * created a regular file in the srctree), then we are
			 * good (t8072).
			 */
			if(variant_get_srctent(tf->variant, pl->dt, &srctent) < 0)
				return -1;
			if(srctent) {
				struct tup_entry *tmp;
				if(tup_db_select_tent_part(srctent->tnode.tupid, pl->pel->path, pl->pel->len, &tmp) < 0)
					return -1;
				if(tmp && tmp->type != TUP_NODE_GHOST) {
					valid_input = 1;
				}
			}

			/* If the file is in the modify list, it is going to be
			 * resurrected, so it is still a valid input (t6053).
			 */
			if(tup_db_in_modify_list(tent->tnode.tupid))
				valid_input = 1;

			if(!valid_input) {
				fprintf(tf->f, "tup error: Explicitly named file '%.*s' in subdir '", pl->pel->len, pl->pel->path);
				print_tupid(tf->f, pl->dt);
				fprintf(tf->f, "' is scheduled to be deleted (possibly the command that created it has been removed).\n");
				return -1;
			}
		}
		args.wildcard = 0;
		if(build_name_list_cb(&args, tent) < 0)
			return -1;
	} else {
		struct tup_entry *srctent = NULL;
		struct tup_entry *dtent;

		if(tup_entry_add(pl->dt, &dtent) < 0)
			return -1;
		if(dtent->type == TUP_NODE_GHOST) {
			fprintf(tf->f, "tup error: Unable to generate wildcard for directory '%s' since it is a ghost.\n", pl->path);
			return -1;
		}

		args.wildcard = 1;
		if(tup_db_select_node_dir_glob(build_name_list_cb, &args, pl->dt, pl->pel->path, pl->pel->len, &tf->g->gen_delete_root, 0) < 0)
			return -1;
		if(variant_get_srctent(tf->variant, pl->dt, &srctent) < 0)
			return -1;
		if(srctent) {
			if(tup_db_select_node_dir_glob(build_name_list_cb, &args, srctent->tnode.tupid, pl->pel->path, pl->pel->len, &tf->g->gen_delete_root, 0) < 0)
				return -1;
		}
	}
	return 0;
}

static int nl_add_bin(struct bin *b, struct name_list *nl)
{
	struct bin_entry *be;
	struct name_list_entry *nle;
	int extlesslen;

	TAILQ_FOREACH(be, &b->entries, list) {
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
	struct tupfile *tf = args->tf;

	/* If the file is generated from another directory, we can't use it in
	 * a wildcard.
	 * 1) srcid == -1 is for normal files
	 * 2) tent->srcid == tf->tupid is for files generated by our Tupfile,
	 *    even if they are in another directory
	 * 3) tent->srcid == tent->dt is for files generated by the directory's
	 *    own Tupfile, which are still wildcard-able.
	 */
	if(tent->srcid != -1 && tent->srcid != tf->tupid && tent->srcid != tent->dt) {
		struct tup_entry *srctent;
		if(args->wildcard)
			return 0;
		if(tup_entry_add(tent->srcid, &srctent) < 0)
			return -1;
		fprintf(tf->f, "tup error: Explicitly named file '");
		print_tup_entry(tf->f, tent);
		fprintf(tf->f, "' can't be listed as an input because it was generated from external directory '");
		print_tup_entry(tf->f, srctent);
		fprintf(tf->f, "' - try placing the file in a group instead and using the group as an input.\n");
		return -1;
	}

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

	/* Do the glob parsing for %g */
	nle->globcnt = glob_parse(args->globstr, args->globstrlen, tent->name.s, nle->glob);

	add_name_list_entry(args->nl, nle);
	return 0;
}

/* Compares a globful string and the subsequent expansion to determine which
 * portions of the string are results of the glob expansion.
 *
 * This function is not guaranteed to give the same results as what sqlite3
 * parsed from the Tupfile. For instance, given the glob str:
 *     a*b*c
 * and the match:
 *     axbybzc
 * the resulting glob sections could be "x","ybz" OR "xby","z".
 * This function will allways try to match the shortest string. In this example
 * it would return the first option.
 *
 * char *base     : pointer to the globful string
 * int baselen    : length of the globful string
 * char *expanded : pointer to the matched string
 * int *globidx   : pointer to an array of index storage
 *
 * Returns: number of globs matched, or -1 on error.
 */
static int glob_parse(const char *pattern, int patlen, char *match, int *globidx)
{
	int p_it = 0;
	int m_it = 0;
	int p2_it;
	int i;
	int glob_cnt = 0;

	/* Two outputs differed, must be a wildcard */
	while(p_it < patlen) {
		while(pattern[p_it] == match[m_it] && p_it < patlen && match[m_it] != '\0') {
			/* Iterate through while the strings are the same */
			p_it++;
			m_it++;
		}

		if (p_it == patlen && match[m_it] == '\0') {
			break;
		}

		/* Found a glob */
		globidx[glob_cnt*2] = m_it;

		/* Handle all of the cases of glob characters */
		if (pattern[p_it] == '[') {
			/* User specified a range of characters. One MUST be matched.
			 * Move p_it to the character after the glob. This will be where
			 * we start looking for the next glob
			 * Need to find closing ']'
			 * Skip first character because it must be there.
			 */
			for (i=2; i<patlen-p_it; i++) {
				if (pattern[p_it+i] == ']') {
					p_it += i + 1;
					break;
				}
			}

			/* Must match one character */
			globidx[glob_cnt*2+1] = 1;
			m_it++;

		} else if (pattern[p_it] == '?') {
			/* Must match one character */
			globidx[glob_cnt*2+1] = 1;
			p_it++;
			m_it++;

		} else {
			int more_wildcards = 0;
			/* Must have found an * */
			p_it++;

			/* Skip any subsequent *. They don't mean anything. */
			while (p_it < patlen && pattern[p_it] == '*') {
				glob_cnt++;
				globidx[glob_cnt*2] = m_it;
				p_it++;
			}

			/* Need to check if there are any glob characters after the *.
			 * Parsing this case is much trickier and not supported yet.
			 */
			for (i=p_it+1; i<patlen; i++) {
				if (pattern[i] == '?' || pattern[i] == '[') {
					more_wildcards = 1;
					break;
				}
			}
			if (more_wildcards) {
				globidx[glob_cnt*2 + 1] = 0;
				glob_cnt++;
				break;

			} else {
				int non_wildcard_len;

				/* Scan for the next glob
				 * p2_it will be the idx of the next glob char
				 */
				p2_it = p_it;
				while (p2_it < patlen) {
					if (pattern[p2_it] == '*' || pattern[p2_it] == '?' || pattern[p2_it] == '[') {
						break;
					}
					p2_it++;
				}

				if (p_it == p2_it) {
					/* Asterisk at the end of the glob string
					 * Match the rest of the matched string
					 */
					globidx[glob_cnt*2 + 1] = strlen(match) - m_it;
					break;
				}

				non_wildcard_len = p2_it - p_it;

				/* Need to start at the end of the expanded string and scan
				 * to the left. This is because:
				 *     pattern:  *.txt
				 *     filename: a.txt.txt
				 * The match should be 'a.txt', not 'a'
				 */
				for (i=strlen(match)-non_wildcard_len; i>=m_it; i--) {
					int c;
					c = strncmp(pattern + p_it, match + i, non_wildcard_len);
					if (c == 0) {
						globidx[glob_cnt*2 + 1] = i - m_it;
						m_it = i;
						break;
					}
				}
			}
		}

		glob_cnt++;
		break;
	}

	return glob_cnt;
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

static int find_existing_command(const struct name_list *onl, tupid_t *cmdid)
{
	struct name_list_entry *onle;
	TAILQ_FOREACH(onle, &onl->entries, list) {
		int rc;
		tupid_t incoming;

		rc = tup_db_get_incoming_link(onle->tent->tnode.tupid, &incoming);
		if(rc < 0)
			return -1;
		if(incoming != -1) {
			*cmdid = incoming;
			return 0;
		}
	}
	*cmdid = -1;
	return 0;
}

static int add_input(struct tupfile *tf, struct tupid_entries *input_root, tupid_t tupid)
{
	struct tup_entry *tent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	if(tent->type == TUP_NODE_GENERATED) {
		struct tupid_entries extra_group_root = {NULL};
		struct tupid_tree *tt;
		tupid_t cmdid;

		if(tupid_tree_add_dup(input_root, tupid) < 0)
			return -1;
		if(tup_db_get_incoming_link(tupid, &cmdid) < 0)
			return -1;
		if(cmdid < 0) {
			fprintf(tf->f, "tup error: Unable to find command id for output file: ");
			print_tup_entry(tf->f, tent);
			fprintf(tf->f, "\n");
			return -1;
		}
		if(tup_db_get_inputs(cmdid, &extra_group_root, NULL, NULL) < 0)
			return -1;
		RB_FOREACH(tt, tupid_entries, &extra_group_root) {
			struct tup_entry *extra_tent;

			if(tup_entry_add(tt->tupid, &extra_tent) < 0)
				return -1;
			if(extra_tent->type == TUP_NODE_GROUP) {
				if(tupid_tree_add_dup(input_root, tt->tupid) < 0)
					return -1;
			}
		}
		free_tupid_tree(&extra_group_root);
	} else if(tent->type == TUP_NODE_VAR || tent->type == TUP_NODE_GROUP) {
		if(tupid_tree_add_dup(input_root, tupid) < 0)
			return -1;
	}
	return 0;
}

static int validate_output(struct tupfile *tf, tupid_t dt, const char *name,
			   const char *fullname, struct tupid_entries *del_root)
{
	struct tup_entry *tent;

	if(tup_db_select_tent(dt, name, &tent) < 0)
		return -1;
	if(tent) {
		if(tent->type == TUP_NODE_GHOST || tent->type == TUP_NODE_GENERATED) {
			int available = 0;
			int rc;
			tupid_t incoming;

			rc = tup_db_get_incoming_link(tent->tnode.tupid, &incoming);
			if(rc < 0)
				return -1;
			if(incoming != -1) {
				if(tupid_tree_search(del_root, incoming) != NULL)
					available = 1;
				if(!available) {
					struct node *n;
					n = find_node(tf->g, tent->srcid);
					if(n != NULL && !n->parsing) {
						available = 1;
					}
				}
				if(!available) {
					fprintf(tf->f, "tup error: Unable to create output file '%s' because it is already owned by command %lli.\n", fullname, incoming);
					tup_db_print(tf->f, incoming);
					return -1;
				}
			}
		} else {
			fprintf(tf->f, "tup error: Attempting to insert '");
			if(dt != tf->tupid) {
				get_relative_dir(tf->f, NULL, NULL, tf->tupid, dt, NULL);
				fprintf(tf->f, "/");
			}
			fprintf(tf->f, "%s' as a generated node when it already exists as a different type (%s). You can do one of two things to fix this:\n  1) If this file is really supposed to be created from the command, delete the file from the filesystem and try again.\n  2) Change your rule in the Tupfile so you aren't trying to overwrite the file.\n", name, tup_db_type(tent->type));
			return -1;
		}
	}
	return 0;
}

static int do_rule_output_pattern(struct tupfile *tf, struct rule *r,
				  struct name_list *nl, const char *ext, int extlen,
				  struct name_list *output_nl)
{
	struct path_list_head oplist;
	char *toutput;
	char *output_pattern;
	char *extra_pattern = NULL;
	char *bang_extra_pattern = NULL;
	char sep[] = "|";
	int rc;

	TAILQ_INIT(&oplist);
	toutput = tup_printf(tf, r->output_pattern, -1, nl, NULL, ext, extlen, NULL);
	if(!toutput)
		return -1;
	output_pattern = eval(tf, toutput, DISALLOW_NODES);
	free(toutput);
	if(!output_pattern)
		return -1;
	if(get_path_list(tf, output_pattern, &oplist, tf->tupid, NULL, 1) < 0)
		return -1;
	/* Insert a fake separator */
	if(get_path_list(tf, sep, &oplist, tf->tupid, NULL, 0) < 0)
		return -1;
	if(r->extra_outputs) {
		extra_pattern = eval(tf, r->extra_outputs, DISALLOW_NODES);
		if(!extra_pattern)
			return -1;
		if(get_path_list(tf, extra_pattern, &oplist, tf->tupid, NULL, 1) < 0)
			return -1;
	}
	if(r->bang_extra_outputs) {
		bang_extra_pattern = eval(tf, r->bang_extra_outputs, DISALLOW_NODES);
		if(!bang_extra_pattern)
			return -1;
		if(get_path_list(tf, bang_extra_pattern, &oplist, tf->tupid, NULL, 1) < 0)
			return -1;
	}
	rc = do_rule(tf, r, nl, &oplist, ext, extlen, output_nl);

	free_path_list(&oplist);
	/* Has to be freed after use of oplist */
	free(output_pattern);
	free(extra_pattern);
	free(bang_extra_pattern);
	return rc;
}

int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
	    struct path_list_head *oplist,
	    const char *ext, int extlen, struct name_list *output_nl)
{
	struct name_list onl;
	struct name_list extra_onl;
	struct name_list_entry *nle, *onle;
	char *tcmd;
	char *cmd;
	struct path_list *pl;
	struct tupid_tree *tt;
	struct tupid_tree *cmd_tt;
	tupid_t cmdid = -1;
	struct tupid_entries input_root = {NULL};
	struct tupid_entries output_root = {NULL};
	int extra_outputs = 0;
	struct tup_entry *tmptent = NULL;
	struct tup_entry *group = NULL;
	struct tup_entry *old_group = NULL;
	int command_modified = 0;

	/* t3017 - empty rules are just pass-through to get the input into the
	 * bin.
	 */
	if(r->command_len == 0) {
		if(r->bin) {
			TAILQ_FOREACH(nle, &nl->entries, list) {
				if(bin_add_entry(r->bin, nle->path, nle->len, nle->tent) < 0)
					return -1;
			}
		}
		return 0;
	}

	init_name_list(&onl);
	init_name_list(&extra_onl);

	while(!TAILQ_EMPTY(oplist)) {
		struct name_list *use_onl;
		struct tup_entry *dest_tent;
		char *newpath;
		char *lastslash;
		pl = TAILQ_FIRST(oplist);

		if(pl->pel->path[0] == '<') {
			if(group) {
				fprintf(tf->f, "tup error: Multiple output groups detected: '");
				print_tup_entry(tf->f, group);
				fprintf(tf->f, "' and '%s/%.*s'\n", pl->path, pl->pel->len, pl->pel->path);
				return -1;
			}

			group = tup_db_create_node_part(pl->dt, pl->pel->path, pl->pel->len, TUP_NODE_GROUP, -1, NULL);
			if(!group)
				return -1;
			goto out_pl;
		}

		if(pl->pel->len == 1 && pl->pel->path[0] == '|') {
			extra_outputs = 1;
			goto out_pl;
		}

		onle = malloc(sizeof *onle);
		if(!onle) {
			parser_error(tf, "malloc");
			return -1;
		}

		/* tup_printf allows %O if we have an onl and are not a command */
		if(extra_outputs) {
			use_onl = &onl;
		} else {
			use_onl = NULL;
		}
		newpath = tup_printf(tf, pl->pel->path, pl->pel->len, nl, use_onl, NULL, 0, NULL);
		if(!newpath) {
			return -1;
		}
		lastslash = strrchr(newpath, PATH_SEP);
		if(lastslash) {
			struct path_element *pel;
			pl->dt = find_dir_tupid_dt(pl->dt, newpath, &pel, SOTGV_CREATE_DIRS, 0);
			free(pel);
		}
		if(pl->path) {
			int plpathlen;
			plpathlen = strlen(pl->path);
			onle->path = malloc(plpathlen + strlen(newpath) + 2);
			strcpy(onle->path, pl->path);
			onle->path[plpathlen] = PATH_SEP;
			onle->path[plpathlen + 1] = 0;
			strcpy(onle->path + plpathlen + 1, newpath);
			free(newpath);
		} else {
			onle->path = newpath;
		}
		if(!onle->path) {
			free(onle);
			return -1;
		}
		if(name_cmp(onle->path, "Tupfile") == 0 ||
		   name_cmp(onle->path, "Tuprules.tup") == 0 ||
		   name_cmp(onle->path, "Tupfile.lua") == 0 ||
		   name_cmp(onle->path, "Tuprules.lua") == 0 ||
		   name_cmp(onle->path, TUP_CONFIG) == 0) {
			fprintf(tf->f, "tup error: Attempted to generate a file called '%s', which is reserved by tup. Your build configuration must be comprised of files you write yourself.\n", onle->path);
			free(onle->path);
			free(onle);
			return -1;
		}
		onle->len = strlen(onle->path);
		onle->extlesslen = onle->len - 1;
		while(onle->extlesslen > 0 && onle->path[onle->extlesslen] != '.')
			onle->extlesslen--;

		if(tf->srctent) {
			struct tup_entry *tent;
			if(tup_db_select_tent(tf->srctent->tnode.tupid, onle->path, &tent) < 0)
				return -1;
			if(tent && tent->type != TUP_NODE_GHOST) {
				fprintf(tf->f, "tup error: Attempting to insert '%s' as a generated node when it already exists as a different type (%s) in the source directory. You can do one of two things to fix this:\n  1) If this file is really supposed to be created from the command, delete the file from the filesystem and try again.\n  2) Change your rule in the Tupfile so you aren't trying to overwrite the file.\n", onle->path, tup_db_type(tent->type));
				return -1;
			}
		}

		if(tup_entry_add(pl->dt, &dest_tent) < 0)
			return -1;

		/* Go up until we find a non-generated dir, so we can try to
		 * gitignore there.
		 */
		while(dest_tent->type == TUP_NODE_GENERATED_DIR) {
			dest_tent = dest_tent->parent;
		}
		if(tupid_tree_add_dup(&tf->g->parse_gitignore_root, dest_tent->tnode.tupid) < 0)
			return -1;

		set_nle_base(onle);
		if(validate_output(tf, pl->dt, onle->base, onle->path, &tf->g->cmd_delete_root) < 0)
			return -1;
		if(tupid_tree_add_dup(&tf->directory_root, pl->dt) < 0)
			return -1;
		onle->tent = tup_db_create_node_part(pl->dt, onle->base, -1,
						     TUP_NODE_GENERATED, tf->tupid, &command_modified);
		if(!onle->tent) {
			free(onle->path);
			free(onle);
			return -1;
		}
		if(tupid_tree_add(&output_root, onle->tent->tnode.tupid) < 0) {
			fprintf(tf->f, "tup error: The output file '%s' is listed multiple times in a command.\n", onle->path);
			return -1;
		}

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
		del_pl(pl, oplist);
	}

	tcmd = tup_printf(tf, r->command, -1, nl, &onl, ext, extlen, r->extra_command);
	if(!tcmd)
		return -1;
	cmd = eval(tf, tcmd, ALLOW_NODES);
	if(!cmd)
		return -1;
	free(tcmd);

	/* If we already have our command string in the db, then use that.
	 * Otherwise, we try to find an existing command of a different
	 * name that points to the output files we are trying to create.
	 * If neither of those cases apply, we just create a new command
	 * node. Note we require a case-sensitive comparison, since we want to
	 * re-run the command if the case of a string or filename has changed.
	 */
	if(tup_db_select_tent(tf->tupid, cmd, &tmptent) < 0)
		return -1;
	if(tmptent && strcmp(tmptent->name.s, cmd) == 0) {
		cmdid = tmptent->tnode.tupid;
		if(tmptent->type != TUP_NODE_CMD) {
			fprintf(tf->f, "tup error: Unable to create command '%s' because the node already exists in the database as type '%s'\n", cmd, tup_db_type(tmptent->type));
			return -1;
		}
	} else {
		if(find_existing_command(&onl, &cmdid) < 0)
			return -1;
		if(cmdid == -1) {
			if(tf->refactoring) {
				fprintf(tf->f, "tup refactoring error: Attempting to create a new command: %s\n", cmd);
				return -1;
			}
			cmdid = create_command_file(tf->tupid, cmd);
		} else {
			if(tf->refactoring) {
				struct tup_entry *old;
				if(tup_entry_add(cmdid, &old) < 0)
					return -1;
				fprintf(tf->f, "tup refactoring error: Attempting to modify a command string:\n");
				fprintf(tf->f, "Old: '%s'\n", old->name.s);
				fprintf(tf->f, "New: '%s'\n", cmd);
				return -1;
			}
			if(tup_db_set_name(cmdid, cmd, tf->tupid) < 0)
				return -1;

			/* Since we changed the name, we have to run the
			 * command again.
			 */
			command_modified = 1;
		}
	}

	free(cmd);
	if(cmdid < 0)
		return -1;

	cmd_tt = malloc(sizeof *cmd_tt);
	if(!cmd_tt) {
		parser_error(tf, "malloc");
		return -1;
	}
	cmd_tt->tupid = cmdid;
	if(tupid_tree_insert(&tf->cmd_root, cmd_tt) < 0) {
		fprintf(tf->f, "tup error: Attempted to add duplicate command ID %lli\n", cmdid);
		tup_db_print(tf->f, cmdid);
		return -1;
	}
	tree_entry_remove(&tf->g->cmd_delete_root, cmdid, &tf->g->cmd_delete_count);
	if(tf->refactoring) {
		tree_entry_remove(&tf->refactoring_cmd_delete_root, cmdid, NULL);
	}

	while(!TAILQ_EMPTY(&onl.entries)) {
		onle = TAILQ_FIRST(&onl.entries);

		if(tup_db_create_unique_link(cmdid, onle->tent->tnode.tupid) < 0) {
			return -1;
		}
		tree_entry_remove(&tf->g->gen_delete_root, onle->tent->tnode.tupid,
				  &tf->g->gen_delete_count);
		if(output_nl) {
			move_name_list_entry(output_nl, &onl, onle);
		} else {
			delete_name_list_entry(&onl, onle);
		}
	}

	while(!TAILQ_EMPTY(&extra_onl.entries)) {
		onle = TAILQ_FIRST(&extra_onl.entries);
		if(tup_db_create_unique_link(cmdid, onle->tent->tnode.tupid) < 0) {
			return -1;
		}
		tree_entry_remove(&tf->g->gen_delete_root, onle->tent->tnode.tupid,
				  &tf->g->gen_delete_count);
		delete_name_list_entry(&extra_onl, onle);
	}

	if(tup_db_write_outputs(tf->f, cmdid, &output_root, group, &old_group, tf->refactoring, command_modified) < 0)
		return -1;
	free_tupid_tree(&output_root);

	TAILQ_FOREACH(nle, &nl->entries, list) {
		if(add_input(tf, &input_root, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	TAILQ_FOREACH(nle, &r->order_only_inputs.entries, list) {
		if(add_input(tf, &input_root, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	TAILQ_FOREACH(nle, &r->bang_oo_inputs.entries, list) {
		if(add_input(tf, &input_root, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	RB_FOREACH(tt, tupid_entries, &tf->env_root) {
		if(add_input(tf, &input_root, tt->tupid) < 0)
			return -1;
	}
	if(group) {
		/* This is a quick check for a simple circular dependency that
		 * can be done before the full check after all parsing is
		 * complete.
		 */
		if(tupid_tree_search(&input_root, group->tnode.tupid) != NULL) {
			fprintf(tf->f, "tup error: Command ID %lli both reads from and writes to this group: ", cmdid);
			print_tup_entry(tf->f, group);
			fprintf(tf->f, "\n");
			tup_db_print(tf->f, cmdid);
			return -1;
		}
	}
	if(tup_db_write_inputs(tf->f, cmdid, &input_root, &tf->env_root, &tf->g->gen_delete_root, group, old_group, tf->refactoring) < 0)
		return -1;
	free_tupid_tree(&input_root);
	return 0;
}

void init_name_list(struct name_list *nl)
{
	TAILQ_INIT(&nl->entries);
	nl->num_entries = 0;
	nl->totlen = 0;
	nl->basetotlen = 0;
	nl->extlessbasetotlen = 0;
	memset(nl->globtotlen, 0, MAX_GLOBS);
	nl->globcnt = 0;
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
	nle->globcnt = 0;
}

static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle)
{
	TAILQ_INSERT_TAIL(&nl->entries, nle, list);
	nl->num_entries++;
	nl->totlen += nle->len;
	nl->basetotlen += nle->baselen;
	nl->extlessbasetotlen += nle->extlessbaselen;
	{
		int i;
		for (i=0; i<nle->globcnt; i++) {
			nl->globtotlen[i] += nle->glob[i*2+1];
		}
	}
	nl->globcnt = nle->globcnt;
}

void delete_name_list(struct name_list *nl)
{
	struct name_list_entry *nle;
	while(!TAILQ_EMPTY(&nl->entries)) {
		nle = TAILQ_FIRST(&nl->entries);
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
	{
		int i;
		for (i=0; i<nle->globcnt; i++) {
			nl->globtotlen[i] -= nle->glob[i*2+1];
		}
	}

	TAILQ_REMOVE(&nl->entries, nle, list);
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
	{
		int i;
		for (i=0; i<nle->globcnt; i++) {
			oldnl->globtotlen[i] -= nle->glob[i*2+1];
		}
	}

	TAILQ_REMOVE(&oldnl->entries, nle, list);

	newnl->num_entries++;
	newnl->totlen += nle->len;
	newnl->basetotlen += nle->baselen;
	newnl->extlessbasetotlen += nle->extlessbaselen;
	{
		int i;
		for (i=0; i<nle->globcnt; i++) {
			newnl->globtotlen[i] += nle->glob[i*2+1];
		}
	}
	newnl->globcnt = nle->globcnt;
	TAILQ_INSERT_TAIL(&newnl->entries, nle, list);
}

static void move_name_list(struct name_list *newnl, struct name_list *oldnl)
{
	struct name_list_entry *nle;

	while(!TAILQ_EMPTY(&oldnl->entries)) {
		nle = TAILQ_FIRST(&oldnl->entries);
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

static char *tup_printf(struct tupfile *tf, const char *cmd, int cmd_len,
			struct name_list *nl, struct name_list *onl,
			const char *ext, int extlen,
			const char *extra_command)
{
	struct name_list_entry *nle;
	char *s;
	int x;
	const char *p;
	const char *next;
	int clen;

	if(!nl) {
		fprintf(tf->f, "tup internal error: tup_printf called with NULL name_list\n");
		return NULL;
	}

	if(cmd_len == -1) {
		cmd_len = strlen(cmd);
	}
	clen = cmd_len;

	if(extra_command) {
		clen += strlen(extra_command) + 1;
	}

	p = cmd;
	while((next = find_char(p, cmd+cmd_len - p, '%')) !=  NULL) {
		int space_chars;

		clen -= 2;
		if(next == cmd+cmd_len-1) {
			fprintf(tf->f, "tup error: Unfinished %%-flag at the end of the string '%s'\n", cmd);
			return NULL;
		}

		next++;
		p = next+1;
		space_chars = nl->num_entries - 1;

		if(*next == 'f') {
			if(nl->num_entries == 0) {
				fprintf(tf->f, "tup error: %%f used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->totlen + space_chars;
		} else if(*next == 'b') {
			if(nl->num_entries == 0) {
				fprintf(tf->f, "tup error: %%b used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->basetotlen + space_chars;
		} else if(*next == 'B') {
			if(nl->num_entries == 0) {
				fprintf(tf->f, "tup error: %%B used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			clen += nl->extlessbasetotlen + space_chars;
		} else if(*next == 'e') {
			if(!ext) {
				fprintf(tf->f, "tup error: %%e is only valid with a foreach rule for files that have extensions.\n");
				if(nl->num_entries == 1) {
					nle = TAILQ_FIRST(&nl->entries);
					fprintf(tf->f, " -- Path: '%s'\n", nle->path);
				} else {
					fprintf(tf->f, " -- This does not appear to be a foreach rule\n");
				}
				return NULL;
			}
			clen += extlen;
		} else if(*next == 'o') {
			if(!onl) {
				fprintf(tf->f, "tup error: %%o can only be used in a command string or extra outputs section.\n");
				return NULL;
			}
			if(onl->num_entries == 0) {
				fprintf(tf->f, "tup error: %%o used in rule pattern and no output files were specified.\n");
				return NULL;
			}
			clen += onl->totlen + (onl->num_entries-1);
		} else if(*next == 'O') {
			struct name_list_entry *onle;
			if(!onl) {
				fprintf(tf->f, "tup error: %%O can only be used in the extra outputs section.\n");
				return NULL;
			}
			if(onl->num_entries != 1) {
				fprintf(tf->f, "tup error: %%O can only be used if there is exactly one output specified.\n");
				return NULL;
			}
			onle = TAILQ_FIRST(&onl->entries);
			clen += onle->extlesslen;
		} else if(*next == 'd') {
			if(tf->tupid == DOT_DT) {
				/* At the top of the tup-hierarchy, we get the
				 * directory from where .tup is stored, since
				 * the top-level tup entry is just "."
				 */
				char *last_slash;
				const char *dirstring;

				last_slash = strrchr(get_tup_top(), PATH_SEP);
				if(last_slash) {
					/* Point to the directory after the last slash */
					dirstring = last_slash + 1;
				} else {
					dirstring = get_tup_top();
				}
				clen += strlen(dirstring);
			} else {
				struct tup_entry *tent;
				if(tup_entry_add(tf->tupid, &tent) < 0)
					return NULL;
				/* Anywhere else in the hierarchy can just use
				 * the last tup entry of the parsed directory
				 * as the %d replacement.
				 */
				clen += tent->name.len;
			}
		} else if(*next == 'g') {
			/* g: Expands to the "glob" portion of an *, ?, [] expansion.
			 *    Given the filnames: a_text.txt, b_text.txt and c_text.txt,
			 *    and the tupfiles:   : foreach *_text.txt |> foo %f |> %g_binary.bin
			 *                        : foreach ?_text.txt |> foo %f |> %g_binary.bin
			 *                        : foreach [abc]_text.txt |> foo %f |> %g_binary.bin
			 *    then outputs ->:    a_binary.bin, b_binary.bin, c_binary.bin
			 */
			if(nl->num_entries == 0) {
				fprintf(tf->f, "tup error: %%g used in rule pattern and no input files were specified.\n");
				return NULL;
			}
			if(nl->num_entries > 1) {
				fprintf(tf->f, "tup error: %%g is only valid with one file.\n");
				return NULL;
			}
			if(nl->globcnt == 0) {
				fprintf(tf->f, "tup error: %%g flag found no globs.\n");
				return NULL;
			}
			clen += nl->globtotlen[0];
		} else if(*next == '<') {
			/* %<group> is expanded by the updater before executing
			 * a command.
			 */
			clen += 2;
		} else if(*next == '%') {
			clen++;
		} else {
			fprintf(tf->f, "tup error: Unknown %%-flag: '%c'\n", *next);
			return NULL;
		}
	}

	s = malloc(clen + 1);
	if(!s) {
		parser_error(tf, "malloc");
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
			TAILQ_FOREACH(nle, &nl->entries, list) {
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
			TAILQ_FOREACH(nle, &nl->entries, list) {
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
			TAILQ_FOREACH(nle, &nl->entries, list) {
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
			TAILQ_FOREACH(nle, &onl->entries, list) {
				if(!first) {
					s[x] = ' ';
					x++;
				}
				memcpy(&s[x], nle->path, nle->len);
				x += nle->len;
				first = 0;
			}
		} else if(*next == 'O') {
			nle = TAILQ_FIRST(&onl->entries);
			memcpy(&s[x], nle->path, nle->extlesslen);
			x += nle->extlesslen;
		} else if(*next == '%') {
			s[x] = '%';
			x++;
		} else if(*next == 'd') {
			if(tf->tupid == DOT_DT) {
				char *last_slash;
				const char *dirstring;
				int len;

				last_slash = strrchr(get_tup_top(), PATH_SEP);
				if(last_slash) {
					dirstring = last_slash + 1;
				} else {
					dirstring = get_tup_top();
				}
				len = strlen(dirstring);
				memcpy(&s[x], dirstring, len);
				x += len;
			} else {
				struct tup_entry *tent;
				if(tup_entry_add(tf->tupid, &tent) < 0)
					return NULL;
				/* Anywhere else in the hierarchy can just use
				 * the last tup entry of the parsed directory
				 * as the %d replacement.
				 */
				memcpy(&s[x], tent->name.s, tent->name.len);
				x += tent->name.len;
			}
		} else if(*next == 'g') {
			TAILQ_FOREACH(nle, &nl->entries, list) {
				memcpy(&s[x], nle->base + nle->glob[0], nle->glob[1]);
				x += nle->glob[1];
			}
		} else if(*next == '<') {
			/* %<group> is expanded by the updater before executing
			 * a command.
			 */
			s[x] = '%';
			x++;
			s[x] = '<';
			x++;
		} else {
			fprintf(tf->f, "tup internal error: Unhandled %%-flag '%c'\n", *next);
			return NULL;
		}
	}
	memcpy(&s[x], p, cmd+cmd_len - p);
	x += cmd+cmd_len - p;

	if(extra_command) {
		int eclen = strlen(extra_command);
		s[x] = ' ';
		x++;
		memcpy(&s[x], extra_command, eclen);
		x += eclen;
	}
	s[x] = 0;
	if((signed)strlen(s) != clen) {
		fprintf(tf->f, "tup internal error: Calculated string length (%i) didn't match actual (%li). String is: '%s'.\n", clen, (long)strlen(s), s);
		return NULL;
	}
	return s;
}

static char *eval(struct tupfile *tf, const char *string, int allow_nodes)
{
	int len = 0;
	char *ret;
	char *p;
	const char *s;
	const char *var;
	const char *syntax_msg = "oops";
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
					syntax_msg = "expected ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				if(rparen-var == 7 &&
				   strncmp(var, "TUP_CWD", 7) == 0) {
					int clen = 0;
					if(get_relative_dir(NULL, NULL, NULL, tf->tupid, tf->curtent->tnode.tupid, &clen) < 0) {
						fprintf(tf->f, "tup internal error: Unable to find relative directory length from ID %lli -> %lli\n", tf->tupid, tf->curtent->tnode.tupid);
						tup_db_print(tf->f, tf->tupid);
						tup_db_print(tf->f, tf->curtent->tnode.tupid);
						return NULL;
					}
					len += clen;
				} else if(rparen - var > 7 &&
					  strncmp(var, "CONFIG_", 7) == 0) {
					const char *atvar;
					atvar = var+7;
					vlen = tup_db_get_varlen(tf->variant, atvar, rparen-atvar);
					if(vlen < 0)
						return NULL;
					len += vlen;
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
					syntax_msg = "expected ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				vlen = tup_db_get_varlen(tf->variant, var, rparen-var);
				if(vlen < 0)
					return NULL;
				len += vlen;
				s = rparen + 1;
			} else {
				s++;
				len++;
			}
		} else if(*s == '&') {
			const char *rparen;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					syntax_msg = "expected ending variable paren ')'";
					goto syntax_error;
				}

				if(allow_nodes != ALLOW_NODES) {
					syntax_msg = "&-variables not allowed here";
					goto syntax_error;
				}

				var = s + 2;
				vlen = nodedb_len(&tf->node_db, var, rparen-var,
				                  tf->curtent->tnode.tupid);
				if (vlen < 0)
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
		parser_error(tf, "malloc");
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
					syntax_msg = "expected ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				if(rparen-var == 7 &&
				   strncmp(var, "TUP_CWD", 7) == 0) {
					int clen = 0;
					if(get_relative_dir(NULL, NULL, p, tf->tupid, tf->curtent->tnode.tupid, &clen) < 0) {
						fprintf(tf->f, "tup internal error: Unable to find relative directory from ID %lli -> %lli\n", tf->tupid, tf->curtent->tnode.tupid);
						tup_db_print(tf->f, tf->tupid);
						tup_db_print(tf->f, tf->curtent->tnode.tupid);
						return NULL;
					}
					p += clen;
				} else if(rparen - var > 7 &&
					  strncmp(var, "CONFIG_", 7) == 0) {
					const char *atvar;
					struct tup_entry *tent;
					atvar = var+7;

					tent = tup_db_get_var(tf->variant, atvar, rparen-atvar, &p);
					if(!tent)
						return NULL;
					if(tupid_tree_add_dup(&tf->input_root, tent->tnode.tupid) < 0)
						return NULL;
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
					syntax_msg = "expected ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				tent = tup_db_get_var(tf->variant, var, rparen-var, &p);
				if(!tent)
					return NULL;
				if(tupid_tree_add_dup(&tf->input_root, tent->tnode.tupid) < 0)
					return NULL;
				s = rparen + 1;
			} else {
				*p = *s;
				p++;
				s++;
			}
		} else if(*s == '&') {
			const char *rparen;

			if(s[1] == '(') {
				rparen = strchr(s+1, ')');
				if(!rparen) {
					syntax_msg = "expected ending variable paren ')'";
					goto syntax_error;
				}

				var = s + 2;
				if (nodedb_copy(&tf->node_db, var, rparen-var, &p,
				                tf->curtent->tnode.tupid) < 0)
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
		fprintf(tf->f, "tup internal error: Length mismatch in eval(): expected %i bytes, wrote %li\n", len, (long)strlen(ret));
		return NULL;
	}

	return ret;

syntax_error:
	fprintf(tf->f, "Syntax error: %s\n", syntax_msg);
	return NULL;
}
