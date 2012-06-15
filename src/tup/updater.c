/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
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

#define _ATFILE_SOURCE
#include "updater.h"
#include "graph.h"
#include "fileio.h"
#include "debug.h"
#include "db.h"
#include "entry.h"
#include "parser.h"
#include "progress.h"
#include "timespan.h"
#include "server.h"
#include "array_size.h"
#include "config.h"
#include "option.h"
#include "container.h"
#include "monitor.h"
#include "path.h"
#include "environ.h"
#include "privs.h"
#include "variant.h"
#include "flist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAX_JOBS 65535

static int check_full_deps_rebuild(void);
static int run_scan(int do_scan);
static int process_config_nodes(int environ_check);
static int process_create_nodes(void);
static int process_update_nodes(int argc, char **argv, int *num_pruned);
static int check_config_todo(void);
static int check_create_todo(void);
static int check_update_todo(int argc, char **argv);
static int build_graph(struct graph *g);
static int add_file_cb(void *arg, struct tup_entry *tent, int style);
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 void *(*work_func)(void *));

static void *create_work(void *arg);
static void *update_work(void *arg);
static void *todo_work(void *arg);
static int update(struct node *n);

static int do_keep_going;
static int num_jobs;
static int full_deps;
static int warnings;
static int show_warnings;

static pthread_mutex_t db_mutex;
static pthread_mutex_t display_mutex;

static const char *signal_err[] = {
	NULL, /* 0 */
	"Hangup detected on controlling terminal or death of controlling process",
	"Interrupt from keyboard",
	"Quit from keyboard",
	"Illegal Instruction",
	NULL, /* 5 */
	"Abort signal from abort(3)",
	NULL,
	"Floating point exception",
	"Kill signal",
	NULL, /* 10 */
	"Segmentation fault",
	NULL,
	"Broken pipe: write to pipe with no readers",
	"Timer signal from alarm(2)",
	"Termination signal", /* 15 */
};

/* The basic gist of these guys is they start out on the free_list, then they
 * move to the active_list once they get setup with 'n' pointing to the node
 * they should process, and their cond variable is signalled. Once they are
 * done processing, they set retn to n and rc to the return code (ie: 0 for
 * success, or an error code) for that node. They then move themselves over to
 * the fin_list and signal the list_cond so that execute_graph() knows this
 * work is done.
 */
struct worker_thread;
LIST_HEAD(worker_thread_head, worker_thread);
struct worker_thread {
	LIST_ENTRY(worker_thread) list;
	pthread_t pid;
	struct graph *g; /* Not for update_work() since it isn't sync'd */

	pthread_mutex_t lock;
	pthread_cond_t cond;

	pthread_mutex_t *list_mutex;
	pthread_cond_t *list_cond;
	struct worker_thread_head *fin_list;
	struct node *n;
	struct node *retn;
	int rc;
	int quit;
};

int updater(int argc, char **argv, int phase)
{
	int x;
	int do_scan = 1;
	int num_pruned = 0;
	int rc = -1;
	int environ_check = 1;

	do_keep_going = tup_option_get_flag("updater.keep_going");
	num_jobs = tup_option_get_int("updater.num_jobs");
	full_deps = tup_option_get_int("updater.full_deps");
	show_warnings = tup_option_get_int("updater.warnings");
	progress_init();

	if(full_deps && !tup_privileged()) {
		fprintf(stderr, "tup error: Unable to support full dependencies since the tup executable is not privileged. Please set the tup executable to be suid root, or if that is not possible then disable the 'updater.full_deps' option. (The option is currently enabled in the file %s)\n", tup_option_get_location("updater.full_deps"));
		return -1;
	}

	if(check_full_deps_rebuild() < 0)
		return -1;

	argc--;
	argv++;

	for(x=0; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("tup.updater");
		} else if(strcmp(argv[x], "--keep-going") == 0 ||
			  strcmp(argv[x], "-k") == 0) {
			do_keep_going = 1;
		} else if(strcmp(argv[x], "--no-keep-going") == 0) {
			do_keep_going = 0;
		} else if(strcmp(argv[x], "--verbose") == 0) {
			tup_entry_set_verbose(1);
		} else if(strcmp(argv[x], "--debug-run") == 0) {
			parser_debug_run();
		} else if(strcmp(argv[x], "--no-scan") == 0) {
			do_scan = 0;
		} else if(strncmp(argv[x], "-j", 2) == 0) {
			num_jobs = strtol(argv[x]+2, NULL, 0);
		} else if(strcmp(argv[x], "--no-environ-check") == 0) {
			environ_check = 0;
		} else if(strcmp(argv[x], "--") == 0) {
			break;
		}
	}

	if(num_jobs < 1) {
		fprintf(stderr, "Warning: Setting the number of jobs to 1\n");
		num_jobs = 1;
	}
	if(num_jobs > MAX_JOBS) {
		fprintf(stderr, "Warning: Setting the number of jobs to MAX_JOBS\n");
		num_jobs = MAX_JOBS;
	}

	if(run_scan(do_scan) < 0)
		return -1;

	if(process_config_nodes(environ_check) < 0)
		goto out;
	if(phase == 1) { /* Collect underpants */
		rc = 0;
		goto out;
	}
	if(process_create_nodes() < 0)
		goto out;
	if(phase == 2) { /* ? */
		rc = 0;
		goto out;
	}
	if(process_update_nodes(argc, argv, &num_pruned) < 0)
		goto out;
	if(num_pruned) {
		tup_main_progress("Partial update complete:");
		printf(" skipped %i commands.\n", num_pruned);
	} else {
		tup_main_progress("Updated.\n");
	}
	rc = 0;
out:
	if(server_quit() < 0)
		rc = -1;
	return rc; /* Profit! */
}

int todo(int argc, char **argv)
{
	int x;
	int rc;
	int do_scan = 1;

	argc--;
	argv++;

	for(x=0; x<argc; x++) {
		if(strcmp(argv[x], "--no-scan") == 0) {
			do_scan = 0;
		} else if(strcmp(argv[x], "--verbose") == 0) {
			tup_entry_set_verbose(1);
		} else if(strcmp(argv[x], "--") == 0) {
			break;
		}
	}

	if(run_scan(do_scan) < 0)
		return -1;

	if(tup_db_begin() < 0)
		return -1;
	rc = check_config_todo();
	if(rc < 0)
		return -1;
	if(rc == 1) {
		printf("Run 'tup read' to proceed to phase 2.\n");
		goto out_ok;
	}

	rc = check_create_todo();
	if(rc < 0)
		return -1;
	if(rc == 1) {
		printf("Run 'tup parse' to proceed to phase 3.\n");
		goto out_ok;
	}

	rc = check_update_todo(argc, argv);
	if(rc < 0)
		return -1;
	if(rc == 1) {
		printf("Run 'tup upd' to bring the system up-to-date.\n");
		goto out_ok;
	}
	printf("tup: Everything is up-to-date.\n");
out_ok:
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int check_full_deps_rebuild(void)
{
	int old_full_deps;

	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_config_get_int("full_deps", -1, &old_full_deps) < 0)
		return -1;
	if(old_full_deps == 0 && full_deps == 1) {
		printf("tup: Full dependency tracking enabled - rebuilding everything!\n");
		if(tup_db_rebuild_all() < 0)
			return -1;
	} else if(old_full_deps == 1 && full_deps == 0) {
		printf("tup: Full dependency tracking disabled - clearing out external dependencies.\n");
		if(tup_db_delete_slash() < 0)
			return -1;
	}
	if(old_full_deps != full_deps)
		if(tup_db_config_set_int("full_deps", full_deps) < 0)
			return -1;
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int run_scan(int do_scan)
{
	int pid;
	int scanned = 0;

	if(monitor_get_pid(0, &pid) < 0) {
		fprintf(stderr, "tup error: Unable to determine if the file monitor is still running.\n");
		return -1;
	}
	if(pid < 0) {
		if(do_scan) {
			tup_main_progress("Scanning filesystem...\n");
			if(tup_scan() < 0)
				return -1;
			scanned = 1;
		} else {
			tup_main_progress("No filesystem scan - user requested --no-scan.\n");
		}
	} else {
		if(full_deps) {
			tup_main_progress("Monitor is running - scanning external dependencies...\n");
			if(tup_external_scan() < 0)
				return -1;
		} else {
			tup_main_progress("No filesystem scan - monitor is running.\n");
		}
	}
	if(!scanned) {
		/* The scanner loads variants, so if we haven't done that yet
		 * then we need to do it here.
		 */
		if(tup_db_begin() < 0)
			return -1;
		if(variant_load() < 0)
			return -1;
		if(tup_db_commit() < 0)
			return -1;
	}
	return 0;
}

static int cleanup_dir(struct tup_entry *tent)
{
	int fd;

	fd = tup_entry_open(tent->parent);
	if(fd < 0) {
		fprintf(stderr, "tup error: Expected to open parent directory of node for possible directory deletion: ");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return -1;
	}
	if(unlinkat(fd, tent->name.s, AT_REMOVEDIR) < 0) {
		perror(tent->name.s);
		fprintf(stderr, "tup error: Unable to clean up old directory in the build tree: \n");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return -1;
	}
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}
	if(tup_del_id_force(tent->tnode.tupid, tent->type) < 0)
		return -1;
	return 0;
}

static int delete_files(struct graph *g)
{
	struct tupid_tree *tt;
	struct tup_entry *tent;
	struct tup_entry_head *entrylist;
	int file_resurrection = 0;
	int rc = -1;

	if(g->cmd_delete_count) {
		char buf[64];
		snprintf(buf, sizeof(buf), "Deleting %i command%s...\n", g->cmd_delete_count, g->cmd_delete_count == 1 ? "" : "s");
		buf[sizeof(buf)-1] = 0;
		tup_show_message(buf);
		start_progress(g->cmd_delete_count, -1, -1);
	}
	while((tt = RB_ROOT(&g->cmd_delete_root)) != NULL) {
		struct tree_entry *te = container_of(tt, struct tree_entry, tnode);

		if(server_is_dead())
			goto out_err;
		if(tup_del_id_force(te->tnode.tupid, te->type) < 0)
			goto out_err;
		skip_result();
		/* Use TUP_NODE_GENERATED to make the bar purple since
		 * we are deleting (not executing) commands.
		 */
		show_progress(-1, TUP_NODE_GENERATED);
		tupid_tree_rm(&g->cmd_delete_root, tt);
		free(te);
	}

	start_progress(g->gen_delete_count, -1, -1);
	entrylist = tup_entry_get_list();
	while((tt = RB_ROOT(&g->gen_delete_root)) != NULL) {
		struct tree_entry *te = container_of(tt, struct tree_entry, tnode);
		int tmp;

		if(server_is_dead())
			goto out_err;

		if(tup_entry_add(tt->tupid, &tent) < 0)
			goto out_err;

		tmp = tup_db_in_modify_list(tt->tupid);
		if(tmp < 0)
			goto out_err;
		if(tmp == 1) {
			tup_entry_list_add(tent, entrylist);
			file_resurrection = 1;
		} else {
			/* Only delete if the file wasn't modified (t6031) */
			show_result(tent, 0, NULL, "rm");
			show_progress(-1, TUP_NODE_GENERATED);

			if(delete_file(tent) < 0)
				goto out_err;
			if(tup_del_id_force(te->tnode.tupid, te->type) < 0)
				goto out_err;
		}
		tupid_tree_rm(&g->gen_delete_root, tt);
		free(te);
	}
	if(file_resurrection) {
		tup_show_message("Converting generated files to normal files...\n");
	}
	LIST_FOREACH(tent, entrylist, list) {
		if(server_is_dead())
			goto out_err;
		if(tent->type == TUP_NODE_GENERATED) {
			if(tup_db_set_type(tent, TUP_NODE_FILE) < 0)
				goto out_err;
			show_result(tent, 0, NULL, "generated -> normal");
			show_progress(-1, TUP_NODE_FILE);
		} else {
			fprintf(stderr, "tup internal error: type of node is %i in delete_files() - should be generated or a directory: ", tent->type);
			print_tup_entry(stderr, tent);
			fprintf(stderr, "\n");
			return -1;
		}
	}

	rc = 0;
out_err:
	tup_entry_release_list();
	return rc;
}

static int delete_in_tree(void)
{
	struct graph g;
	if(create_graph(&g, TUP_NODE_FILE) < 0)
		return -1;
	if(tup_db_type_to_tree(&g.cmd_delete_root, &g.cmd_delete_count, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_type_to_tree(&g.gen_delete_root, &g.gen_delete_count, TUP_NODE_GENERATED) < 0)
		return -1;
	if(delete_files(&g) < 0)
		return -1;
	if(destroy_graph(&g) < 0)
		return -1;
	return 0;
}

static void initialize_server_struct(struct server *s, struct tup_entry *tent)
{
	s->id = tent->tnode.tupid;
	s->exited = 0;
	s->signalled = 0;
	s->exit_status = -1;
	s->exit_sig = -1;
	s->output_fd = -1;
	s->error_fd = -1;
	s->error_mutex = &display_mutex;
	init_file_info(&s->finfo, tup_entry_variant(tent)->variant_dir);
}

static int check_empty_variant(struct tup_entry *tent)
{
	int fd;
	struct flist f = {0, 0, 0};
	int displayed_error = 0;

	fd = tup_entry_open(tent);
	if(!fd) {
		fprintf(stderr, "tup error: Unable to open variant directory for checking: ");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return -1;
	}
	if(fchdir(fd) < 0) {
		perror("fchdir");
		return -1;
	}
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}
	flist_foreach(&f, ".") {
		if(f.filename[0] == '.')
			continue;
		if(strcmp(f.filename, TUP_CONFIG) == 0)
			continue;
		if(!displayed_error) {
			fprintf(stderr, "tup error: Variant directory must only contain a tup.config file. Found extra files:\n");
			displayed_error = 1;
		}
		fprintf(stderr, " - %s\n", f.filename);
	}
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir(tup_top_fd())");
		return -1;
	}
	if(displayed_error) {
		fprintf(stderr, "tup error: Please clean out the variant directory of extra files, or remove the tup.config from this variant: ");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return -1;
	}
	return 0;
}

static int delete_variant_dirs(struct tup_entry *tent)
{
	struct variant *variant;
	LIST_FOREACH(variant, get_variant_list(), list) {
		if(!variant->root_variant) {
			struct tup_entry *cleanup_tent;

			if(tup_db_select_tent(variant->tent->dt, tent->name.s, &cleanup_tent) < 0)
				return -1;
			if(cleanup_tent) {
				if(tup_db_delete_variant(cleanup_tent, NULL, NULL) < 0)
					return -1;
				if(cleanup_dir(cleanup_tent) < 0)
					return -1;
			}
		}
	}
	return 0;
}

static int is_valid_variant_tent(struct tup_entry *tent)
{
	/* If the tup.config file was deleted, the node becomes a ghost until
	 * we delete it in process_config_nodes().
	 */
	if(tent->type == TUP_NODE_GHOST)
		return 0;

	/* If the variant directory was moved with the monitor running, the
	 * build directory no longer has DOT_DT as a parent. We can also remove
	 * the variant in this case (t8063).
	 */
	if(tent->parent && tent->parent->dt != DOT_DT)
		return 0;
	return 1;
}

static int process_config_nodes(int environ_check)
{
	struct graph g;
	struct tup_entry *vartent;
	struct node *n;
	struct variant *root_variant;
	int variants_removed = 0;
	int using_variants = 0;
	int using_in_tree = 0;
	int new_variants = 0;

	if(tup_db_begin() < 0)
		return -1;
	if(create_graph(&g, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_CONFIG) < 0)
		return -1;

	/* Pop off the root node since we don't use it here. */
	while(!LIST_EMPTY(&g.root->edges)) {
		remove_edge(LIST_FIRST(&g.root->edges));
	}
	TAILQ_REMOVE(&g.node_list, g.root, list);
	remove_node(&g, g.root);

	{
		/* The variants that are already loaded will either have:
		 * A) No variants at all (this is the first update, or the last variant was removed)
		 * B) Just a root variant (using_in_tree = 1)
		 * C) One or more real variants. Ghost nodes here correspond to
		 *    variants that are about to be deleted. (using_variants = 1)
		 */
		struct variant *variant;
		LIST_FOREACH(variant, get_variant_list(), list) {
			if(!variant->root_variant) {
				if(is_valid_variant_tent(variant->tent)) {
					using_variants = 1;
					break;
				}
			} else {
				using_in_tree = 1;
				break;
			}
		}
	}

	if(!TAILQ_EMPTY(&g.plist)) {
		if(server_init(SERVER_CONFIG_MODE) < 0)
			return -1;
		if(environ_check)
			tup_main_progress("Reading in new configuration/environment variables...\n");
		else
			tup_main_progress("Reading in new configuration variables (environment check disabled)...\n");

		start_progress(g.num_nodes, -1, -1);
		while(!TAILQ_EMPTY(&g.plist)) {
			struct variant *variant;
			int rm_node = 1;
			struct parser_server ps;

			n = TAILQ_FIRST(&g.plist);
			TAILQ_REMOVE(&g.plist, n, list);

			variant = variant_search(n->tent->dt);

			if(!is_valid_variant_tent(n->tent) && n->tent->dt != DOT_DT) {
				/* tup.config deleted */
				struct tup_entry *parent = n->tent->parent;

				/* Reset the build directory's variant link. We
				 * may have set it in tup_file_missing().
				 */
				parent->variant = NULL;
				if(variant) {
					/* If we had a variant corresponding to
					 * this tup.config, then we need to
					 * wipe out all of our @-variables,
					 * remove the varaint, remove our
					 * tup.config node, and delete the
					 * variant tree. We also add the
					 * variant directory to the create_list
					 * so it can be propagated to other
					 * variants (if it still exists).
					 */
					show_result(n->tent, 0, NULL, "delete variant");
					if(tup_db_delete_tup_config(n->tent) < 0)
						return -1;
					if(variant_rm(variant) < 0)
						return -1;
					if(delete_name_file(n->tent->tnode.tupid) < 0)
						return -1;
					if(tup_db_delete_variant(parent, NULL, NULL) < 0)
						return -1;
					if(parent->type == TUP_NODE_GHOST) {
						if(delete_name_file(parent->tnode.tupid) < 0)
							return -1;
					} else {
						if(tup_db_add_create_list(parent->tnode.tupid) < 0)
							return -1;
					}
					variants_removed = 1;
				} else {
					/* We can get here if we created a
					 * tup.config in the wrong place and
					 * then move it out of the way, so a
					 * variant for it is never created (and
					 * therefore shouldn't be removed - see
					 * t8047).
					 */
					if(tup_db_unflag_config(n->tent->tnode.tupid) < 0)
						return -1;
					show_result(n->tent, 0, NULL, "clean-up node");
				}
			} else {
				/* tup.config created or modified, or root
				 * tup.config deleted.
				 */
				int rc;

				if(n->tent->parent->srcid != DOT_DT) {
					if(tup_db_set_srcid(n->tent->parent, DOT_DT) < 0)
						goto err_rollback;
				}

				variant = variant_search(n->tent->dt);
				if(variant == NULL) {
					if(n->tent->dt != DOT_DT) {
						new_variants = 1;
						if(check_empty_variant(n->tent->parent) < 0)
							return -1;
						if(delete_variant_dirs(n->tent->parent) < 0)
							return -1;
					}

					if(variant_add(n->tent, 1, &variant) < 0)
						goto err_rollback;
					if(tup_db_add_variant_list(n->tent->tnode.tupid) < 0)
						goto err_rollback;
					TAILQ_INSERT_HEAD(&g.node_list, n, list);
					rm_node = 0;
					show_result(n->tent, 0, NULL, "new variant");
				} else {
					if(!variant->enabled) {
						/* If the variant is disabled
						 * it's because we removed the
						 * build dir and then
						 * re-created it before getting
						 * through process_config_nodes
						 * (t8055).
						 */
						new_variants = 1;
						TAILQ_INSERT_HEAD(&g.node_list, n, list);
						rm_node = 0;
						if(variant_enable(variant) < 0)
							return -1;
					}
					show_result(n->tent, 0, NULL, "updated variant");
				}
				compat_lock_disable();
				initialize_server_struct(&ps.s, n->tent);
				if(server_parser_start(&ps) < 0)
					goto err_rollback;
				rc = tup_db_read_vars(ps.root_fd, n->tent->dt, TUP_CONFIG, n->tent->tnode.tupid, variant->vardict_file);
				if(server_parser_stop(&ps) < 0)
					goto err_rollback;
				if(rc < 0)
					goto err_rollback;
				if(add_config_files(&ps.s.finfo, n->tent) < 0)
					goto err_rollback;
				compat_lock_enable();

				if(tup_db_unflag_config(n->tent->tnode.tupid) < 0)
					goto err_rollback;
			}

			if(rm_node) {
				/* Remove the link from the root node to us. */
				while(!LIST_EMPTY(&n->incoming)) {
					remove_edge(LIST_FIRST(&n->incoming));
				}
				remove_node(&g, n);
			}
			show_progress(-1, TUP_NODE_FILE);
		}
	} else {
		if(environ_check)
			tup_main_progress("Reading in new environment variables...\n");
		else
			tup_main_progress("No tup.config changes (environment check disabled).\n");
	}

	root_variant = variant_search(DOT_DT);
	if(!root_variant) {
		int enabled = 0;

		if(tup_db_get_tup_config_tent(&vartent) < 0)
			goto err_rollback;

		if(variant_list_empty()) {
			/* Going from variant to in-tree */
			if(tup_db_add_variant_list(vartent->tnode.tupid) < 0)
				goto err_rollback;

			enabled = 1;
		}
		if(variant_add(vartent, enabled, NULL) < 0)
			goto err_rollback;
	} else {
		if(using_in_tree && new_variants) {
			if(delete_in_tree() < 0)
				goto err_rollback;
		}
		if(using_variants || new_variants) {
			if(tup_db_unflag_variant(root_variant->tent->tnode.tupid) < 0)
				goto err_rollback;
			root_variant->enabled = 0;
		}
	}

	/* If we have created an in-tree variant (either a real tup.config
	 * based one or a fake placeholder one), and variants were removed by
	 * the scanner/monitor at some point, then we need to reparse all the
	 * Tupfiles to create the in-tree build.
	 */
	if(!using_variants && variants_removed) {
		if(tup_db_reparse_all() < 0)
			return -1;
	}

	TAILQ_FOREACH(n, &g.node_list, list) {
		if(n->tent->dt != DOT_DT) {
			if(tup_db_duplicate_directory_structure(n->tent->parent) < 0)
				goto err_rollback;
		}
	}

	if(tup_db_check_env(environ_check) < 0)
		goto err_rollback;
	if(destroy_graph(&g) < 0)
		return -1;
	tup_db_commit();
	clear_progress();

	return 0;

err_rollback:
	tup_db_rollback();
	return -1;
}

static struct tup_entry *get_rel_tent(struct tup_entry *base, struct tup_entry *tent)
{
	struct tup_entry *new;
	struct tup_entry *sub;
	int do_mkdir = 0;

	if(!tent->parent)
		return base;

	new = get_rel_tent(base, tent->parent);
	if(!new)
		return NULL;

	sub = tup_db_create_node_srcid(new->tnode.tupid, tent->name.s, TUP_NODE_DIR, tent->tnode.tupid, &do_mkdir);
	if(!sub) {
		fprintf(stderr, "tup error: Unable to create tup node for variant directory: ");
		print_tup_entry(stderr, base);
		fprintf(stderr, "\n");
		return NULL;
	}
	if(do_mkdir) {
		int fd;
		fd = tup_entry_open(new);
		if(fd < 0)
			return NULL;
		if(mkdirat(fd, tent->name.s, 0777) < 0) {
			if(errno != EEXIST) {
				perror(tent->name.s);
				fprintf(stderr, "tup error: Unable to create sub-directory in variant tree.\n");
				return NULL;
			}
		}
		if(close(fd) < 0) {
			perror("close(fd)");
			return NULL;
		}
	}
	return sub;
}

static int rm_variant_dir_cb(void *arg, struct tup_entry *tent)
{
	struct graph *g = arg;
	struct node *n;
	n = find_node(g, tent->tnode.tupid);
	if(n) {
		if(n->state == STATE_REMOVING) {
			TAILQ_REMOVE(&g->removing_list, n, list);
		} else {
			TAILQ_REMOVE(&g->plist, n, list);
		}
		while(!LIST_EMPTY(&n->incoming)) {
			remove_edge(LIST_FIRST(&n->incoming));
		}
		remove_node(g, n);
		g->num_nodes--;
	}
	return 0;
}

static int process_create_nodes(void)
{
	struct graph g;
	struct node *n;
	struct node *tmp;
	int rc;

	tup_db_begin();
	if(create_graph(&g, TUP_NODE_DIR) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	TAILQ_FOREACH_SAFE(n, &g.plist, list, tmp) {
		struct variant *node_variant = tup_entry_variant(n->tent);

		if(n->tent->type != TUP_NODE_DIR)
			continue;

		if(node_variant->root_variant) {
			struct variant *variant;
			LIST_FOREACH(variant, get_variant_list(), list) {
				/* Add in all other variants to parse */
				if(!variant->root_variant) {
					struct tup_entry *new_tent;
					new_tent = get_rel_tent(variant->tent->parent, n->tent);
					if(!new_tent) {
						fprintf(stderr, "tup internal error: Unable to find directory for variant '%s' for subdirectory: ", variant->variant_dir);
						print_tup_entry(stderr, n->tent);
						fprintf(stderr, "\n");
						return -1;
					}
					if(add_file_cb(&g, new_tent, TUP_LINK_NORMAL) < 0)
						return -1;
				}
			}
			if(!node_variant->enabled) {
				g.num_nodes--;
			}
		} else {
			struct tup_entry *srctent;
			int force_removal = 0;
			if(tup_entry_add(n->tent->srcid, &srctent) < 0) {
				return -1;
			}
			/* If the srctent is no longer a directory, that means
			 * our reason for existing is gone. Force our removal.
			 */
			if(srctent->type != TUP_NODE_DIR)
				force_removal = 1;
			/* If we are a variant subdirectory (our srcid
			 * is not DOT_DT), and our name doesn't match
			 * our srcid, that means the srctree was
			 * renamed, and we go away.
			 */
			if(n->tent->srcid != DOT_DT &&
			   strcmp(srctent->name.s, n->tent->name.s) != 0) {
				force_removal = 1;
			}

			if(force_removal) {
				if(tup_db_select_node_by_link(add_file_cb, &g, n->tent->tnode.tupid) < 0)
					return -1;
				TAILQ_REMOVE(&g.plist, n, list);
				TAILQ_INSERT_TAIL(&g.removing_list, n, list);
				n->state = STATE_REMOVING;
			}
		}
	}

	while(!TAILQ_EMPTY(&g.removing_list)) {
		struct tup_entry *cleanup_tent;
		n = TAILQ_FIRST(&g.removing_list);
		cleanup_tent = n->tent;

		/* The rm_variant_dir_cb will remove the node from the removing_list */
		if(tup_db_delete_variant(n->tent, &g, rm_variant_dir_cb) < 0)
			return -1;
		if(cleanup_dir(cleanup_tent) < 0)
			return -1;
	}

	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes) {
		tup_main_progress("Parsing Tupfiles...\n");
	} else {
		tup_main_progress("No Tupfiles to parse.\n");
	}
	if(graph_empty(&g)) {
		tup_main_progress("No files to delete.\n");
		goto out_destroy;
	}

	if(server_init(SERVER_PARSER_MODE) < 0) {
		return -1;
	}
	/* create_work must always use only 1 thread since no locking is done */
	compat_lock_disable();
	rc = execute_graph(&g, 0, 1, create_work);
	compat_lock_enable();
	if(rc == 0) {
		if(g.gen_delete_count) {
			tup_main_progress("Deleting files...\n");
		} else {
			tup_main_progress("No files to delete.\n");
		}
		rc = delete_files(&g);
	}
	if(rc < 0) {
		tup_db_rollback();
		if(rc != -1)
			fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}

out_destroy:
	if(destroy_graph(&g) < 0)
		return -1;
	tup_db_commit();
	return 0;
}

static int process_update_nodes(int argc, char **argv, int *num_pruned)
{
	struct graph g;
	int rc = 0;

	tup_db_begin();
	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;

	if(prune_graph(&g, argc, argv, num_pruned) < 0)
		return -1;

	if(g.num_nodes) {
		tup_main_progress("Executing Commands...\n");
	} else {
		tup_main_progress("No commands to execute.\n");
	}
	/* If the graph only has the root node, just bail. Note that this
	 * may be different from g.num_nodes==0, since that only counts
	 * command nodes (we may have to go through the DAG and clear out
	 * some files marked modify that don't actually point to commands).
	 */
	if(graph_empty(&g))
		goto out_destroy;

	if(pthread_mutex_init(&db_mutex, NULL) != 0) {
		perror("pthread_mutex_init");
		return -1;
	}
	if(pthread_mutex_init(&display_mutex, NULL) != 0) {
		perror("pthread_mutex_init");
		return -1;
	}
	warnings = 0;
	if(server_init(SERVER_UPDATER_MODE) < 0) {
		return -1;
	}
	rc = execute_graph(&g, do_keep_going, num_jobs, update_work);
	if(warnings) {
		fprintf(stderr, "tup warning: Update resulted in %i warning%s\n", warnings, warnings == 1 ? "" : "s");
	}
	if(rc == -2) {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	pthread_mutex_destroy(&display_mutex);
	pthread_mutex_destroy(&db_mutex);
out_destroy:
	if(destroy_graph(&g) < 0)
		return -1;
	tup_db_commit();
	return rc;
}

static int check_config_todo(void)
{
	struct graph g;
	int rc;
	int stuff_todo = 0;

	if(create_graph(&g, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_CONFIG) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	g.total_mtime = -1;
	if(g.num_nodes) {
		printf("Tup phase 1: The following tup.config files must be parsed:\n");
		stuff_todo = 1;
	}
	rc = execute_graph(&g, 0, 1, todo_work);
	if(rc == 0) {
		rc = stuff_todo;
	} else if(rc == -1) {
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}

	if(destroy_graph(&g) < 0)
		return -1;
	return rc;
}

static int check_create_todo(void)
{
	struct graph g;
	int rc;
	int stuff_todo = 0;

	if(create_graph(&g, TUP_NODE_DIR) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(g.num_nodes) {
		printf("Tup phase 2: The following directories must be parsed:\n");
		stuff_todo = 1;
	}
	rc = execute_graph(&g, 0, 1, todo_work);
	if(rc == 0) {
		rc = stuff_todo;
	} else if(rc == -1) {
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return rc;
}

static int check_update_todo(int argc, char **argv)
{
	struct graph g;
	int rc;
	int stuff_todo = 0;
	int num_pruned = 0;

	if(create_graph(&g, TUP_NODE_CMD) < 0)
		return -1;
	if(tup_db_select_node_by_flags(add_file_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(prune_graph(&g, argc, argv, &num_pruned) < 0)
		return -1;
	if(g.num_nodes) {
		printf("Tup phase 3: The following %i command%s will be executed:\n", g.num_nodes, g.num_nodes == 1 ? "" : "s");
		stuff_todo = 1;
	}
	rc = execute_graph(&g, 0, 1, todo_work);
	if(rc == 0) {
		rc = stuff_todo;
	} else if(rc == -1) {
		return -1;
	} else {
		fprintf(stderr, "tup error: execute_graph returned %i - abort. This is probably a bug.\n", rc);
		return -1;
	}
	if(num_pruned) {
		printf("Partial update: %i command%s will be skipped.\n", num_pruned, num_pruned == 1 ? "" : "s");
	}
	if(destroy_graph(&g) < 0)
		return -1;
	return rc;
}

static int build_graph(struct graph *g)
{
	struct node *cur;

	while(!TAILQ_EMPTY(&g->plist)) {
		cur = TAILQ_FIRST(&g->plist);
		if(cur->state == STATE_INITIALIZED) {
			DEBUGP("find deps for node: %lli\n", cur->tnode.tupid);
			g->cur = cur;
			if(tup_db_select_node_by_link(add_file_cb, g, cur->tnode.tupid) < 0)
				return -1;
			cur->state = STATE_PROCESSING;
		} else if(cur->state == STATE_PROCESSING) {
			DEBUGP("remove node from stack: %lli\n", cur->tnode.tupid);
			TAILQ_REMOVE(&g->plist, cur, list);
			TAILQ_INSERT_TAIL(&g->node_list, cur, list);
			cur->state = STATE_FINISHED;
		} else if(cur->state == STATE_FINISHED) {
			fprintf(stderr, "tup internal error: STATE_FINISHED node %lli in plist\n", cur->tnode.tupid);
			tup_db_print(stderr, cur->tnode.tupid);
			return -1;
		}
	}

	return 0;
}

static int add_file_cb(void *arg, struct tup_entry *tent, int style)
{
	struct graph *g = arg;
	struct node *n;

	n = find_node(g, tent->tnode.tupid);
	if(n != NULL)
		goto edge_create;
	n = create_node(g, tent);
	if(!n)
		return -1;

edge_create:
	if(n->state == STATE_PROCESSING) {
		/* A circular dependency is not guaranteed to trigger this,
		 * but it is easy to check before going through the graph.
		 */
		fprintf(stderr, "tup error: Circular dependency detected! "
			"Last edge was: %lli -> %lli\n",
			g->cur->tnode.tupid, tent->tnode.tupid);
		return -1;
	}
	if(style & TUP_LINK_NORMAL && n->expanded == 0) {
		if(n->tent->type == g->count_flags || g->count_flags < 0) {
			g->num_nodes++;
			if(g->total_mtime != -1) {
				if(n->tent->mtime == -1)
					g->total_mtime = -1;
				else
					g->total_mtime += n->tent->mtime;
			}
		}
		n->expanded = 1;
		TAILQ_REMOVE(&g->node_list, n, list);
		TAILQ_INSERT_HEAD(&g->plist, n, list);
	}

	if(create_edge(g->cur, n, style) < 0)
		return -1;
	return 0;
}

static void pop_node(struct graph *g, struct node *n)
{
	while(!LIST_EMPTY(&n->edges)) {
		struct edge *e;
		e = LIST_FIRST(&n->edges);
		if(e->dest->state != STATE_PROCESSING) {
			/* Put the node back on the plist, and mark it as such
			 * by changing the state to STATE_PROCESSING.
			 */
			TAILQ_REMOVE(&g->node_list, e->dest, list);
			TAILQ_INSERT_HEAD(&g->plist, e->dest, list);
			e->dest->state = STATE_PROCESSING;
		}

		remove_edge(e);
	}
	remove_node(g, n);
}

/* Returns:
 *   0: everything built ok
 *  -1: a command failed
 *  -2: a system call failed (some work threads may still be active)
 */
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 void *(*work_func)(void *))
{
	struct node *root;
	struct worker_thread *workers;
	int rc = -1;
	int x;
	int active = 0;
	int failed = 0;
	pthread_mutex_t list_mutex;
	pthread_cond_t list_cond;
	struct worker_thread_head active_list;
	struct worker_thread_head fin_list;
	struct worker_thread_head free_list;

	LIST_INIT(&active_list);
	LIST_INIT(&fin_list);
	LIST_INIT(&free_list);
	if(pthread_mutex_init(&list_mutex, NULL) != 0) {
		perror("pthread_mutex_init");
		return -2;
	}
	if(pthread_cond_init(&list_cond, NULL) != 0) {
		perror("pthread_cond_init");
		return -2;
	}

	workers = malloc(sizeof(*workers) * jobs);
	if(!workers) {
		perror("malloc");
		return -2;
	}
	for(x=0; x<jobs; x++) {
		workers[x].g = g;

		if(pthread_mutex_init(&workers[x].lock, NULL) != 0) {
			perror("pthread_mutex_init");
			return -2;
		}
		if(pthread_cond_init(&workers[x].cond, NULL) != 0) {
			perror("pthread_cond_init");
			return -2;
		}

		workers[x].list_mutex = &list_mutex;
		workers[x].list_cond = &list_cond;
		workers[x].fin_list = &fin_list;
		workers[x].n = NULL;
		workers[x].retn = NULL;
		workers[x].rc = -1;
		workers[x].quit = 0;
		LIST_INSERT_HEAD(&free_list, &workers[x], list);

		if(pthread_create(&workers[x].pid, NULL, work_func, &workers[x]) < 0) {
			perror("pthread_create");
			return -2;
		}
	}

	root = TAILQ_FIRST(&g->node_list);
	DEBUGP("root node: %lli\n", root->tnode.tupid);
	TAILQ_REMOVE(&g->node_list, root, list);
	pop_node(g, root);

	start_progress(g->num_nodes, g->total_mtime, jobs);
	/* Keep going as long as:
	 * 1) There is work to do (plist is not empty)
	 * 2) The server hasn't been killed
	 * 3) No jobs have failed, or if jobs have failed we have keep_going set.
	 */
	while(!TAILQ_EMPTY(&g->plist) && !server_is_dead() && (!failed || keep_going)) {
		struct node *n;
		struct worker_thread *wt;
		n = TAILQ_FIRST(&g->plist);
		DEBUGP("cur node: %lli\n", n->tnode.tupid);
		if(!LIST_EMPTY(&n->incoming)) {
			/* Here STATE_FINISHED means we're on the node_list,
			 * therefore not ready for processing.
			 */
			TAILQ_REMOVE(&g->plist, n, list);
			TAILQ_INSERT_HEAD(&g->node_list, n, list);
			n->state = STATE_FINISHED;
			goto check_empties;
		}

		if(!n->expanded) {
			TAILQ_REMOVE(&g->plist, n, list);
			pop_node(g, n);
			goto check_empties;
		}

		TAILQ_REMOVE(&g->plist, n, list);
		active++;

		wt = LIST_FIRST(&free_list);
		pthread_mutex_lock(&list_mutex);
		LIST_REMOVE(wt, list);
		LIST_INSERT_HEAD(&active_list, wt, list);
		pthread_mutex_unlock(&list_mutex);

		pthread_mutex_lock(&wt->lock);
		wt->n = n;
		wt->rc = -1;
		pthread_cond_signal(&wt->cond);
		pthread_mutex_unlock(&wt->lock);

check_empties:
		/* Keep looking for dudes to return as long as:
		 *  1) There are no more free workers
		 *  2) There is no work to do (plist is empty or the server is
		 *     dead or we failed without keep-going) and some people
		 *     are active.
		 */
		while(LIST_EMPTY(&free_list) ||
		      ((TAILQ_EMPTY(&g->plist) || server_is_dead() || (failed && !keep_going)) && active)) {
			pthread_mutex_lock(&list_mutex);
			while(LIST_EMPTY(&fin_list)) {
				pthread_cond_wait(&list_cond, &list_mutex);
			}
			wt = LIST_FIRST(&fin_list);
			n = wt->retn;
			wt->retn = NULL;
			LIST_REMOVE(wt, list);
			LIST_INSERT_HEAD(&free_list, wt, list);
			pthread_mutex_unlock(&list_mutex);
			active--;

			if(wt->rc == 0) {
				pop_node(g, n);
			} else {
				/* Failed jobs sit on the node_list until the
				 * graph is destroyed.
				 */
				TAILQ_INSERT_TAIL(&g->node_list, n, list);
				failed++;
			}
		}
	}
	clear_progress();
	if(failed) {
		fprintf(stderr, " *** tup: %i job%s failed.\n", failed, failed == 1 ? "" : "s");
		if(keep_going)
			fprintf(stderr, " *** tup: Remaining nodes skipped due to errors in command execution.\n");
	} else if(!TAILQ_EMPTY(&g->node_list) || !TAILQ_EMPTY(&g->plist)) {
		if(server_is_dead()) {
			fprintf(stderr, " *** tup: Remaining nodes skipped due to caught signal.\n");
		} else {
			struct node *n;
			fprintf(stderr, "fatal tup error: Graph is not empty after execution. This likely indicates a circular dependency.\n");
			fprintf(stderr, "Node list:\n");
			TAILQ_FOREACH(n, &g->node_list, list) {
				fprintf(stderr, " Node[%lli]: %s\n", n->tnode.tupid, n->tent->name.s);
			}
			fprintf(stderr, "plist:\n");
			TAILQ_FOREACH(n, &g->plist, list) {
				fprintf(stderr, " Plist[%lli]: %s\n", n->tnode.tupid, n->tent->name.s);
			}
		}
	} else {
		rc = 0;
	}

	/* First tell all the threads to quit */
	for(x=0; x<jobs; x++) {
		pthread_mutex_lock(&workers[x].lock);
		workers[x].quit = 1;
		pthread_cond_signal(&workers[x].cond);
		pthread_mutex_unlock(&workers[x].lock);
	}
	/* Then wait for all the threads to quit */
	for(x=0; x<jobs; x++) {
		pthread_join(workers[x].pid, NULL);
		pthread_cond_destroy(&workers[x].cond);
		pthread_mutex_destroy(&workers[x].lock);
	}

	free(workers); /* Viva la revolucion! */
	pthread_mutex_destroy(&list_mutex);
	pthread_cond_destroy(&list_cond);
	return rc;
}

static struct node *worker_wait(struct worker_thread *wt)
{
	pthread_mutex_lock(&wt->lock);
	while(wt->n == NULL && wt->quit == 0) {
		pthread_cond_wait(&wt->cond, &wt->lock);
	}
	pthread_mutex_unlock(&wt->lock);
	if(wt->quit)
		return (void*)-1;
	return wt->n;
}

static void worker_ret(struct worker_thread *wt, int rc)
{
	wt->rc = rc;
	wt->retn = wt->n;
	wt->n = NULL;

	pthread_mutex_lock(wt->list_mutex);
	LIST_REMOVE(wt, list);
	LIST_INSERT_HEAD(wt->fin_list, wt, list);
	pthread_cond_signal(wt->list_cond);
	pthread_mutex_unlock(wt->list_mutex);
}

static void *create_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct graph *g = wt->g;
	struct node *n;

	while(1) {
		int rc = 0;

		n = worker_wait(wt);
		if(n == (void*)-1)
			break;

		if(n->tent->type == TUP_NODE_DIR) {
			if(tup_entry_variant(n->tent)->enabled) {
				if(n->already_used) {
					rc = 0;
				} else {
					rc = parse(n, g, NULL);
				}
				show_progress(-1, TUP_NODE_DIR);
			}
		} else if(n->tent->type == TUP_NODE_VAR ||
			  n->tent->type == TUP_NODE_FILE ||
			  n->tent->type == TUP_NODE_GENERATED ||
			  n->tent->type == TUP_NODE_CMD) {
			rc = 0;
		} else {
			fprintf(stderr, "tup error: Unknown node type %i with ID %lli named '%s' in create graph.\n", n->tent->type, n->tnode.tupid, n->tent->name.s);
			rc = -1;
		}
		if(tup_db_unflag_create(n->tnode.tupid) < 0)
			rc = -1;

		worker_ret(wt, rc);
	}
	return NULL;
}

static void *update_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct node *n;
	static int jobs_active = 0;

	while(1) {
		struct edge *e;
		int rc = 0;

		n = worker_wait(wt);
		if(n == (void*)-1)
			break;

		if(n->tent->type == TUP_NODE_CMD) {
			pthread_mutex_lock(&display_mutex);
			jobs_active++;
			show_progress(jobs_active, TUP_NODE_CMD);
			pthread_mutex_unlock(&display_mutex);

			rc = update(n);

			pthread_mutex_lock(&display_mutex);
			jobs_active--;
			show_progress(jobs_active, TUP_NODE_CMD);
			pthread_mutex_unlock(&display_mutex);

			/* If the command succeeds, mark any next commands (ie:
			 * our output files' output links) as modify in case we
			 * hit an error. Note we don't just mark the output
			 * file as modify since we aren't actually changing the
			 * file. Doing so would muddy the semantics of the
			 * modify list, which is needed in order to convert
			 * generated files to normal files (t6035).
			 */
			if(rc == 0) {
				pthread_mutex_lock(&db_mutex);
				LIST_FOREACH(e, &n->edges, list) {
					struct edge *f;

					LIST_FOREACH(f, &e->dest->edges, list) {
						if(f->style & TUP_LINK_NORMAL) {
							if(tup_db_add_modify_list(f->dest->tnode.tupid) < 0)
								rc = -1;
						}
					}
				}
				if(tup_db_unflag_modify(n->tnode.tupid) < 0)
					rc = -1;
				pthread_mutex_unlock(&db_mutex);
			}
		} else {
			pthread_mutex_lock(&db_mutex);
			/* Mark the next nodes as modify in case we hit
			 * an error - we'll need to pick up there (t6006).
			 */
			LIST_FOREACH(e, &n->edges, list) {
				if(e->style & TUP_LINK_NORMAL) {
					if(tup_db_add_modify_list(e->dest->tnode.tupid) < 0)
						rc = -1;
				}
			}
			if(tup_db_unflag_modify(n->tnode.tupid) < 0)
				rc = -1;

			/* For environment variables, if there are no more
			 * out-going edges, then this variable is no longer
			 * needed and we can remove it. Note this is a lazy
			 * removal, since this is triggered only if an
			 * environment variable has been modified.
			 */
			if(n->tent->type == TUP_NODE_VAR &&
			   n->tent->dt == env_dt() &&
			   LIST_EMPTY(&n->edges) &&
			   rc == 0) {
				rc = delete_name_file(n->tent->tnode.tupid);
			}
			pthread_mutex_unlock(&db_mutex);
		}

		worker_ret(wt, rc);
	}
	return NULL;
}

static void *todo_work(void *arg)
{
	struct worker_thread *wt = arg;
	struct graph *g = wt->g;
	struct node *n;

	while(1) {
		n = worker_wait(wt);
		if(n == (void*)-1)
			break;

		if(n->tent->type == g->count_flags || g->count_flags == -1) {
			show_result(n->tent, 0, NULL, NULL);
		}

		worker_ret(wt, 0);
	}
	return NULL;
}

static int unlink_outputs(int dfd, struct node *n)
{
	struct edge *e;
	struct node *output;
	LIST_FOREACH(e, &n->edges, list) {
		output = e->dest;
		if(unlinkat(dfd, output->tent->name.s, 0) < 0) {
			if(errno != ENOENT) {
				pthread_mutex_lock(&display_mutex);
				show_result(n->tent, 1, NULL, NULL);
				perror("unlinkat");
				fprintf(stderr, "tup error: Unable to unlink previous output file: %s\n", output->tent->name.s);
				pthread_mutex_unlock(&display_mutex);
				return -1;
			}
		}
	}
	return 0;
}

static int process_output(struct server *s, struct tup_entry *tent,
			  struct tupid_entries *sticky_root,
			  struct tupid_entries *normal_root,
			  struct timespan *ts)
{
	FILE *f;
	int is_err = 1;
	struct timespan *show_ts = NULL;
	time_t ms = -1;
	int *warning_dest;

	if(show_warnings)
		warning_dest = &warnings;
	else
		warning_dest = NULL;

	f = tmpfile();
	if(!f) {
		show_result(tent, 1, NULL, NULL);
		perror("tmpfile");
		fprintf(stderr, "tup error: Unable to open the error log for writing.\n");
		return -1;
	}
	if(s->exited) {
		if(s->exit_status == 0) {
			if(write_files(f, tent->tnode.tupid, &s->finfo, warning_dest, 0, sticky_root, normal_root, full_deps, tup_entry_vardt(tent)) < 0) {
				fprintf(f, " *** Command ID=%lli ran successfully, but tup failed to save the dependencies.\n", tent->tnode.tupid);
			} else {
				timespan_end(ts);
				show_ts = ts;
				ms = timespan_milliseconds(ts);

				/* Hooray! */
				is_err = 0;
			}
		} else {
			fprintf(f, " *** Command ID=%lli failed with return value %i\n", tent->tnode.tupid, s->exit_status);
			if(write_files(f, tent->tnode.tupid, &s->finfo, warning_dest, 1, sticky_root, normal_root, full_deps, tup_entry_vardt(tent)) < 0) {
				fprintf(f, " *** Additionally, command %lli failed to process input dependencies. These should probably be fixed before addressing the command failure.\n", tent->tnode.tupid);
			}
		}
	} else if(s->signalled) {
		int sig = s->exit_sig;
		const char *errmsg = "Unknown signal";

		if(sig >= 0 && sig < ARRAY_SIZE(signal_err) && signal_err[sig])
			errmsg = signal_err[sig];
		fprintf(f, " *** Command ID=%lli killed by signal %i (%s)\n", tent->tnode.tupid, sig, errmsg);
	} else {
		fprintf(f, "tup internal error: Expected s->exited or s->signalled to be set for command ID=%lli", tent->tnode.tupid);
	}

	fflush(f);
	rewind(f);

	show_result(tent, is_err, show_ts, NULL);
	if(display_output(s->output_fd, is_err ? 3 : 0, tent->name.s, 0) < 0)
		return -1;
	if(close(s->output_fd) < 0) {
		perror("close(s->output_fd)");
		return -1;
	}
	if(display_output(fileno(f), 2, tent->name.s, 0) < 0)
		return -1;
	if(fclose(f) != 0) {
		perror("fclose");
		return -1;
	}

	if(is_err)
		return -1;
	if(tent->mtime != ms)
		if(tup_db_set_mtime(tent, ms) < 0)
			return -1;
	return 0;
}

static int update(struct node *n)
{
	int dfd = -1;
	const char *name = n->tent->name.s;
	struct server s;
	int rc;
	struct tupid_entries sticky_root = {NULL};
	struct tupid_entries normal_root = {NULL};
	struct tup_env newenv;
	struct timespan ts;
	int do_chroot = full_deps;

	timespan_start(&ts);
	if(name[0] == '^') {
		name++;
		while(*name && *name != ' ' && *name != '^') {
			switch(*name) {
				case 'c':
					if(!tup_privileged()) {
						pthread_mutex_lock(&display_mutex);
						show_result(n->tent, 1, NULL, NULL);
						fprintf(stderr, "tup error: Attempting to run a sub-process in a chroot, but tup is not privileged. Please set the tup executable to be suid root, or if that is not possible then remove the ^c flag in the command: %s\n", n->tent->name.s);
						pthread_mutex_unlock(&display_mutex);
						return -1;
					}
					do_chroot = 1;
					break;
				default:
					pthread_mutex_lock(&display_mutex);
					show_result(n->tent, 1, NULL, NULL);
					fprintf(stderr, "tup error: Unknown ^ flag: '%c'\n", *name);
					pthread_mutex_unlock(&display_mutex);
					return -1;
			}
			name++;
		}
		while(*name && *name != '^') name++;
		if(!*name) {
			pthread_mutex_lock(&display_mutex);
			show_result(n->tent, 1, NULL, NULL);
			fprintf(stderr, "tup error: Missing ending '^' flag in command %lli: %s\n", n->tnode.tupid, n->tent->name.s);
			pthread_mutex_unlock(&display_mutex);
			return -1;
		}
		name++;
		while(isspace(*name)) name++;
	}

	dfd = tup_entry_open(n->tent->parent);
	if(dfd < 0) {
		pthread_mutex_lock(&display_mutex);
		show_result(n->tent, 1, NULL, NULL);
		fprintf(stderr, "tup error: Unable to open directory for update work.\n");
		tup_db_print(stderr, n->tent->parent->tnode.tupid);
		pthread_mutex_unlock(&display_mutex);
		goto err_out;
	}

	if(unlink_outputs(dfd, n) < 0)
		goto err_close_dfd;

	pthread_mutex_lock(&db_mutex);
	rc = tup_db_get_links(n->tent->tnode.tupid, &sticky_root, &normal_root);
	if(rc == 0)
		rc = tup_db_get_environ(&sticky_root, &normal_root, &newenv);
	initialize_server_struct(&s, n->tent);
	pthread_mutex_unlock(&db_mutex);
	if(rc < 0)
		goto err_close_dfd;

	if(server_exec(&s, dfd, name, &newenv, n->tent->parent, do_chroot) < 0) {
		pthread_mutex_lock(&display_mutex);
		fprintf(stderr, " *** Command ID=%lli failed: %s\n", n->tnode.tupid, name);
		pthread_mutex_unlock(&display_mutex);
		goto err_close_dfd;
	}
	environ_free(&newenv);
	if(close(dfd) < 0) {
		perror("close(dfd)");
		return -1;
	}

	pthread_mutex_lock(&db_mutex);
	pthread_mutex_lock(&display_mutex);
	rc = process_output(&s, n->tent, &sticky_root, &normal_root, &ts);
	pthread_mutex_unlock(&display_mutex);
	pthread_mutex_unlock(&db_mutex);
	free_tupid_tree(&sticky_root);
	free_tupid_tree(&normal_root);
	if(server_postexec(&s) < 0)
		return -1;
	return rc;

err_close_dfd:
	if(close(dfd) < 0) {
		perror("close(dfd)");
	}
err_out:
	return -1;
}
