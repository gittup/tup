/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2020  Mike Shal <marfey@gmail.com>
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
#include "variant.h"
#include "flist.h"
#include "estring.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAX_JOBS 65535

typedef int(*worker_function)(struct graph *g, struct node *n);

static int check_full_deps_rebuild(void);
static int run_scan(int do_scan);
static int process_config_nodes(int environ_check);
static int process_create_nodes(void);
static int process_update_nodes(int argc, char **argv, int *num_pruned);
static int check_config_todo(void);
static int check_create_todo(void);
static int check_update_todo(int argc, char **argv);
static int execute_graph(struct graph *g, int keep_going, int jobs,
			 worker_function work_func);

static void *run_thread(void *arg);

static int create_work(struct graph *g, struct node *n);
static int update_work(struct graph *g, struct node *n);
static int generate_work(struct graph *g, struct node *n);
static int todo_work(struct graph *g, struct node *n);
static int expand_command(char **res,
			  struct tup_entry *tent, const char *cmd,
			  struct tent_entries *group_sticky_root,
			  struct tent_entries *used_groups_root);
static int update(struct node *n);

static int do_keep_going;
static int num_jobs;
static int full_deps;
static int warnings;
static int show_warnings;
static int refactoring;
static int verbose;

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
	worker_function fn;

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
	full_deps = tup_option_get_flag("updater.full_deps");
	show_warnings = tup_option_get_flag("updater.warnings");
	progress_init();

	if(check_full_deps_rebuild() < 0)
		return -1;

	argc--;
	argv++;

	for(x=0; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("tup.updater");
		} else if(strcmp(argv[x], "--verbose") == 0) {
			verbose = 1;
			tup_entry_set_verbose(1);
		} else if(strcmp(argv[x], "--debug-run") == 0) {
			parser_debug_run();
		} else if(strcmp(argv[x], "--no-scan") == 0) {
			do_scan = 0;
		} else if(strcmp(argv[x], "--no-environ-check") == 0) {
			environ_check = 0;
		} else if(strcmp(argv[x], "--debug-logging") == 0) {
			logging_enable(argc, argv);
		} else if(strcmp(argv[x], "--quiet") == 0 ||
			  strcmp(argv[x], "-q") == 0) {
			progress_quiet();
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

	if(phase < 0) {
		phase = -phase;
		refactoring = 1;
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

static int generate_script_mkdir(FILE *f, struct tupid_entries *root, struct tup_entry *dtent)
{
	if(dtent->type != TUP_NODE_GENERATED_DIR)
		return 0;
	if(tupid_tree_search(root, dtent->tnode.tupid) != NULL)
		return 0;
	tupid_tree_add(root, dtent->tnode.tupid);
	if(generate_script_mkdir(f, root, dtent->parent) < 0)
		return -1;
#ifdef _WIN32
	fprintf(f, "mkdir \"");
#else
	fprintf(f, "mkdir -p \"");
#endif
	print_tup_entry(f, dtent);
	fprintf(f, "\"\n");
	return 0;
}

static struct tup_entry *generate_cwd;
static FILE *generate_f;
int generate(int argc, char **argv)
{
	struct graph g;
	struct node *n;
	struct node *tmp;
	struct tup_entry *vartent;
	struct tup_entry *varfiletent;
	struct tupid_entries generated_dir_root;
	char *script_name = NULL;
	char *config_file = NULL;
	int verbose_script = 0;
	int is_batch_script = 0;
	int x;
	int rc;
	const char *file_open_flags = "w";
#ifdef _WIN32
	const char *example_script = "script_name.bat | script_name.sh";
#else
	const char *example_script = "script_name.sh";
#endif

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "--config") == 0) {
			if(x+1 >= argc) {
				fprintf(stderr, "--config requires a filename");
				return -1;
			}
			x++;
			config_file = argv[x];
			continue;
		} else if(strcmp(argv[x], "--verbose") == 0) {
			verbose_script = 1;
			continue;
		}
		if(script_name) {
			fprintf(stderr, "Usage: tup generate [--config config_file] %s\n", example_script);
			return -1;
		}
		script_name = argv[x];
	}
	if(!script_name) {
		fprintf(stderr, "Usage: tup generate [--config config_file] %s\n", example_script);
		return -1;
	}
#ifdef _WIN32
	int len;
	len = strlen(script_name);
	if(len >= 4) {
		if(strcmp(script_name + len - 4, ".bat") == 0) {
			is_batch_script = 1;
		} else {
			/* We have to use '/' as the separator for shell
			 * scripts, since we normally invoke things with
			 * CreateProcess, which understands the backslash as a
			 * path seperator. However, sh does not.
			 */
			set_path_sep('/');

			/* We can't use DOS line-endings in shell scripts. */
			file_open_flags = "wb";
		}
	}
#endif
	if(tup_db_create(0, 1) < 0)
		return -1;
	if(tup_option_init(argc, argv) < 0)
		return -1;
	if(open_tup_top() < 0)
		return -1;
	printf("Scanning...\n");
	if(tup_scan() < 0)
		return -1;

	tup_db_begin();
	printf("Reading tup.config...\n");
	if(tup_db_get_tup_config_tent(&vartent) < 0)
		return -1;
	if(variant_add(vartent, 1, NULL) < 0)
		return -1;
	if(config_file) {
		tupid_t sub_dir_dt;

		sub_dir_dt = get_sub_dir_dt();
		if(sub_dir_dt < 0)
			return -1;
		varfiletent = get_tent_dt(sub_dir_dt, config_file);
		if(!varfiletent) {
			fprintf(stderr, "Unable to find tupid for: '%s'\n", config_file);
				return -1;
		}
	} else {
		varfiletent = vartent;
	}
	if(tup_db_read_vars(tup_top_fd(), varfiletent->parent, varfiletent->name.s, vartent, NULL) < 0)
		return -1;

	printf("Parsing...\n");
	if(create_graph(&g, TUP_NODE_DIR, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	start_progress(g.num_nodes, g.total_mtime, 1);

	/* The parsing nodes have to be removed so that we know if dependent
	 * Tupfiles have already been parsed.
	 */
	n = TAILQ_FIRST(&g.node_list);
	TAILQ_REMOVE(&g.node_list, n, list);
	while(!LIST_EMPTY(&n->edges)) {
		remove_edge(LIST_FIRST(&n->edges));
	}
	remove_node(&g, n);

	TAILQ_FOREACH_SAFE(n, &g.plist, list, tmp) {
		if(!n->already_used)
			if(parse(n, &g, NULL, 0, 0, full_deps) < 0)
				return -1;
		TAILQ_REMOVE(&g.plist, n, list);
		while(!LIST_EMPTY(&n->incoming)) {
			remove_edge(LIST_FIRST(&n->incoming));
		}
		remove_node(&g, n);
	}
	if(destroy_graph(&g) < 0)
		return -1;

	printf("Generate: %s\n", script_name);
	if(chdir(get_tup_top()) < 0) {
		perror("chdir(get_tup_top())\n");
		return -1;
	}
	generate_f = fopen(script_name, file_open_flags);
	if(!generate_f) {
		perror(script_name);
		fprintf(stderr, "tup error: Unable to open script for writing.\n");
		return -1;
	}
	if(is_batch_script) {
		fprintf(generate_f, "@echo %s\n", verbose_script ? "ON" : "OFF");
	} else {
		fprintf(generate_f, "#! /bin/sh -e%s\n", verbose_script ? "x" : "");
	}
	if(create_graph(&g, TUP_NODE_CMD, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;

	/* Loop through all the nodes and pull out any generated directories.
	 * The script will 'mkdir' all of these at the top of the script.
	 */
	RB_INIT(&generated_dir_root);
	TAILQ_FOREACH(n, &g.node_list, list) {
		if(n->tent->type == TUP_NODE_GENERATED) {
			if(generate_script_mkdir(generate_f, &generated_dir_root, n->tent->parent) < 0)
				return -1;
		}
	}
	free_tupid_tree(&generated_dir_root);

	if(tup_entry_add(DOT_DT, &generate_cwd) < 0)
		return -1;
	rc = execute_graph(&g, 0, 1, generate_work);
	if(rc < 0)
		return -1;
	fclose(generate_f);
	chmod(script_name, 0755);
	if(destroy_graph(&g) < 0)
		return -1;
	tup_db_commit();
	if(tup_db_close() < 0)
		return -1;
	if(close(tup_top_fd()) < 0) {
		perror("close(tup_top_fd())");
	}
	return 0;
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
	struct tent_list_head save_list;
	struct tent_list *tlist;
	int file_resurrection = 0;
	int rc = -1;

	tent_list_init(&save_list);
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
		skip_result(NULL);
		/* Use TUP_NODE_GENERATED to make the bar purple since
		 * we are deleting (not executing) commands.
		 */
		show_progress(-1, TUP_NODE_GENERATED);
		tupid_tree_rm(&g->cmd_delete_root, tt);
		free(te);
	}

	start_progress(g->gen_delete_count, -1, -1);
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
			if(tent_list_add_tail(&save_list, tent) < 0)
				goto out_err;
			file_resurrection = 1;
		} else {
			/* Only delete if the file wasn't modified (t6031) */
			show_result(tent, 0, NULL, "rm", 1);
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
	tent_list_foreach(tlist, &save_list) {
		tent = tlist->tent;
		if(server_is_dead())
			goto out_err;
		if(tent->type == TUP_NODE_GENERATED) {
			if(tup_db_modify_cmds_by_input(tent->tnode.tupid) < 0)
				goto out_err;
			if(tup_db_delete_links(tent->tnode.tupid) < 0)
				goto out_err;
			if(tup_db_set_type(tent, TUP_NODE_FILE) < 0)
				goto out_err;
			log_debug_tent("Convert generated -> normal", tent, "\n");
			show_result(tent, 0, NULL, "generated -> normal", 1);
			show_progress(-1, TUP_NODE_FILE);
		} else {
			fprintf(stderr, "tup internal error: type of node is %i in delete_files() - should be a generated file: ", tent->type);
			print_tup_entry(stderr, tent);
			fprintf(stderr, "\n");
			return -1;
		}
	}

	rc = 0;
out_err:
	free_tent_list(&save_list);
	return rc;
}

static int delete_in_tree(void)
{
	struct graph g;
	if(create_graph(&g, TUP_NODE_FILE, -1) < 0)
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

static int initialize_server_struct(struct server *s, struct tup_entry *tent)
{
	s->id = tent->tnode.tupid;
	s->exited = 0;
	s->signalled = 0;
	s->exit_status = -1;
	s->exit_sig = -1;
	s->output_fd = -1;
	s->error_fd = -1;
	s->error_mutex = &display_mutex;
	if(init_file_info(&s->finfo, tup_entry_variant(tent)->variant_dir, server_unlink()) < 0)
		return -1;

	if(tent->type == TUP_NODE_CMD) {
		if(tup_db_get_inputs(tent->tnode.tupid, &s->finfo.sticky_root, &s->finfo.normal_root, &s->finfo.group_sticky_root) < 0)
			return -1;
		if(tup_db_get_outputs(tent->tnode.tupid, &s->finfo.output_root, &s->finfo.exclusion_root, NULL) < 0)
			return -1;
	}
	return 0;
}

static int check_empty_variant(struct tup_entry *tent)
{
	int fd;
	struct flist f = FLIST_INITIALIZER;
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

			if(tup_db_select_tent(variant->tent->parent, tent->name.s, &cleanup_tent) < 0)
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

static int recurse_delete_variant(struct tup_entry *tent)
{
	/* If we had a variant corresponding to this tup.config, then we need
	 * to wipe out all of our @-variables, remove the varaint, remove our
	 * tup.config node, and delete the variant tree. We also add the
	 * variant directory to the create_list so it can be propagated to
	 * other variants (if it still exists).
	 */
	struct tup_entry *parent = tent->parent;

	if(delete_name_file(tent->tnode.tupid) < 0)
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
	return 0;
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
	/* Use TUP_NODE_ROOT to count everything */
	if(create_graph(&g, TUP_NODE_ROOT, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_CONFIG) < 0)
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

				/* Reset the build directory's variant link. We
				 * may have set it in tup_file_missing().
				 */
				n->tent->parent->variant = NULL;
				if(variant) {
					if(variant_rm(variant) < 0)
						return -1;
					if(tup_db_delete_tup_config(n->tent) < 0)
						return -1;

					if(n->tent->parent->dt == DOT_DT) {
						show_result(n->tent, 0, NULL, "delete variant", 1);
						if(recurse_delete_variant(n->tent) < 0)
							return -1;
					} else {
						/* The variant directory was
						 * moved to the source tree,
						 * and the monitor saw it.
						 * We've already deleted the
						 * variant and @-variables, so
						 * just unset the config flag,
						 * and all the generated nodes
						 * will become normal nodes
						 * during the regular
						 * generated -> normal code.
						 */
						show_result(n->tent, 0, NULL, "discard variant", 1);
						if(tup_db_unflag_config(n->tent->tnode.tupid) < 0)
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
					show_result(n->tent, 0, NULL, "clean-up node", 1);
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
					show_result(n->tent, 0, NULL, "new variant", 1);
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
					show_result(n->tent, 0, NULL, "updated variant", 1);
				}
				compat_lock_disable();
				if(initialize_server_struct(&ps.s, n->tent) < 0)
					goto err_rollback;
				if(server_parser_start(&ps) < 0)
					goto err_rollback;
				rc = tup_db_read_vars(ps.root_fd, n->tent->parent, TUP_CONFIG, n->tent, variant->vardict_file);
				if(server_parser_stop(&ps) < 0)
					goto err_rollback;
				if(rc < 0)
					goto err_rollback;
				if(add_config_files(&ps.s.finfo, n->tent, full_deps) < 0)
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

	sub = tup_db_create_node_srcid(new, tent->name.s, TUP_NODE_DIR, tent->tnode.tupid, &do_mkdir);
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

static int mark_variant_dir_for_deletion(struct graph *g, struct node *n)
{
	if(tup_db_select_node_by_link(build_graph_cb, g, n->tent->tnode.tupid) < 0)
		return -1;
	TAILQ_REMOVE(&g->plist, n, list);
	TAILQ_INSERT_TAIL(&g->removing_list, n, list);
	n->state = STATE_REMOVING;
	return 0;
}

static int gitignore(tupid_t tupid)
{
	int fd_old, fd_new;
	int dfd;
	struct tup_entry *tent;
	struct tup_entry *gitignore_tent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	if(tup_db_select_tent(tent, ".gitignore", &gitignore_tent) < 0)
		return -1;
	if(gitignore_tent && gitignore_tent->type == TUP_NODE_GENERATED) {
		const char *tg_str = "##### TUP GITIGNORE #####\n";
		int tg_idx = 0;
		int copied_tg_str = 0;
		char nextchar;
		struct stat buf;
		FILE *f;
		dfd = tup_entry_open(tent);
		if(dfd < 0)
			return -1;
		fd_new = openat(dfd, ".gitignore.new", O_CREAT|O_WRONLY|O_TRUNC, 0666);
		if(fd_new < 0) {
			perror(".gitignore");
			goto err_out;
		}
		f = fdopen(fd_new, "w");
		if(!f) {
			perror("fdopen");
			goto err_out;
		}
		fd_old = openat(dfd, ".gitignore", O_RDONLY);
		if(fd_old < 0 && errno != ENOENT) {
			perror(".gitignore");
			goto err_close;
		}
		if(fd_old >= 0) {
			while(1) {
				int rc;

				rc = read(fd_old, &nextchar, 1);
				if(rc < 0) {
					perror("read");
					goto err_close_both;
				}
				if(rc == 0) {
					break;
				}

				if(tg_str[tg_idx] == nextchar) {
					tg_idx++;
				} else {
					tg_idx = 0;
				}
				if(fprintf(f, "%c", nextchar) < 0) {
					perror("fprintf");
					goto err_close_both;
				}
				if(tg_idx == 26) {
					copied_tg_str = 1;
					break;
				}
			}
			if(close(fd_old) < 0) {
				perror("close(fd_old)");
				goto err_close;
			}
		}
		if(!copied_tg_str) {
			if(fprintf(f, "%s", tg_str) < 0) {
				perror("fprintf");
				goto err_close;
			}
		}
		if(fprintf(f, "##### Lines below automatically generated by Tup.\n") < 0) {
			perror("fprintf");
			goto err_close;
		}
		if(fprintf(f, "##### Do not edit.\n") < 0) {
			perror("fprintf");
			goto err_close;
		}
		if(tupid == 1) {
			if(fprintf(f, ".tup\n") < 0) {
				perror("fprintf");
				goto err_close;
			}
		}
		if(tup_db_write_gitignore(f, tupid) < 0)
			goto err_close;
		fclose(f);
		if(renameat(dfd, ".gitignore.new", dfd, ".gitignore") < 0) {
			perror("move(.gitignore)");
			goto err_out;
		}
		if(fstatat(dfd, ".gitignore", &buf, AT_SYMLINK_NOFOLLOW) < 0) {
			perror("fstatat(.gitignore)");
			goto err_out;
		}
		if(tup_db_set_mtime(gitignore_tent, MTIME(buf)) < 0)
			goto err_out;

		close(dfd);
	}
	return 0;

err_close_both:
	close(fd_old);
err_close:
	close(fd_new);
err_out:
	close(dfd);
	fprintf(stderr, "tup error: Unable to create the .gitignore file in directory: ");
	print_tup_entry(stderr, tent);
	fprintf(stderr, "\n");
	return -1;
}

static int process_create_nodes(void)
{
	struct graph g;
	struct node *n;
	struct node *tmp;
	struct tupid_tree *tt;
	int rc;
	int old_changes = 0;

	if(refactoring)
		old_changes = tup_db_changes();

	tup_db_begin();
	if(create_graph(&g, TUP_NODE_DIR, TUP_NODE_GHOST) < 0)
		return -1;
	/* Force total_mtime to -1 so we count directory nodes rather than use
	 * their mtimes to determine progress. In some cases we may assign an
	 * mtime to a directory, which can make things misleading.
	 */
	g.total_mtime = -1;

	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_CREATE) < 0)
		return -1;
	TAILQ_FOREACH_SAFE(n, &g.plist, list, tmp) {
		struct variant *node_variant = tup_entry_variant(n->tent);

		if(n->tent->type != TUP_NODE_DIR)
			continue;
		if(is_virtual_tent(n->tent))
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
					if(build_graph_cb(&g, new_tent) < 0)
						return -1;
				}
			}
			if(!node_variant->enabled) {
				g.num_nodes--;
			}
		} else {
			struct tup_entry *srctent;
			int force_removal = 0;

			if(n->tent->srcid == -1) {
				/* Ignore directories that we manually created inside
				 * the variant directory (eg: mkdir build/tmpdir)
				 */
			} else if(n->tent->srcid == VARIANT_SRCDIR_REMOVED) {
				if(mark_variant_dir_for_deletion(&g, n) < 0)
					return -1;
			} else {
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
					if(mark_variant_dir_for_deletion(&g, n) < 0)
						return -1;
				}
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
	log_graph(&g, "create");
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
		if(rc == 0 && group_need_circ_check()) {
			/* TODO: Extra space */
			tup_show_message("Checking circular dependencies among groups...\n");
			if(group_circ_check() < 0)
				rc = -1;
		}
		if(rc == 0 && !RB_EMPTY(&g.normal_dir_root)) {
			int msg_shown = 0;
			while(!RB_EMPTY(&g.normal_dir_root)) {
				int dbrc;
				tt = RB_MIN(tupid_entries, &g.normal_dir_root);
				dbrc = tup_db_is_generated_dir(tt->tupid);
				if(dbrc < 0)
					return -1;
				if(dbrc) {
					struct tup_entry *tent;
					if(!msg_shown) {
						msg_shown = 1;
						tup_show_message("Converting normal directories to generated directories...\n");
					}
					if(tup_entry_add(tt->tupid, &tent) < 0)
						return -1;
					printf("tup: Converting ");
					print_tup_entry(stdout, tent);
					printf(" to a generated directory.\n");
					if(tup_db_normal_dir_to_generated(tent) < 0)
						return -1;
					/* Also check the parent. */
					if(tupid_tree_add_dup(&g.normal_dir_root, tent->dt) < 0)
						return -1;
					/* And check if the parent needs us in
					 * gitignore.
					 */
					if(tupid_tree_add_dup(&g.parse_gitignore_root, tent->dt) < 0)
						return -1;
				}
				tupid_tree_rm(&g.normal_dir_root, tt);
				free(tt);
			}
		}
		if(rc == 0 && !RB_EMPTY(&g.parse_gitignore_root) && !refactoring) {
			tup_show_message("Generating .gitignore files...\n");
			RB_FOREACH(tt, tupid_entries, &g.parse_gitignore_root) {
				if(gitignore(tt->tupid) < 0) {
					rc = -1;
					break;
				}
			}
			free_tupid_tree(&g.parse_gitignore_root);
		}
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

	if(refactoring) {
		if(tup_db_changes() != old_changes) {
			fprintf(stderr, "tup error: The database changed while refactoring. Tup should have caught this change before here and reported it as an error. Please run 'tup refactor --debug-sql' and send the output to the tup-users@googlegroups.com mailing list.\n");
			tup_db_rollback();
			return -1;
		}
	}
	tup_db_commit();
	return 0;
}

static int process_update_nodes(int argc, char **argv, int *num_pruned)
{
	struct graph g;
	int rc = 0;

	tup_db_begin();
	if(create_graph(&g, TUP_NODE_CMD, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;

	if(prune_graph(&g, argc, argv, num_pruned, GRAPH_PRUNE_GENERATED, verbose) < 0)
		return -1;

	log_graph(&g, "update");
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

	/* Use TUP_NODE_ROOT to count everything */
	if(create_graph(&g, TUP_NODE_ROOT, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_CONFIG) < 0)
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

	if(create_graph(&g, TUP_NODE_DIR, TUP_NODE_GHOST) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_CREATE) < 0)
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

	if(create_graph(&g, TUP_NODE_CMD, -1) < 0)
		return -1;
	if(tup_db_select_node_by_flags(build_graph_cb, &g, TUP_FLAGS_MODIFY) < 0)
		return -1;
	if(build_graph(&g) < 0)
		return -1;
	if(prune_graph(&g, argc, argv, &num_pruned, GRAPH_PRUNE_GENERATED, verbose) < 0)
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
			 worker_function work_func)
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
		workers[x].fn = work_func;
		LIST_INSERT_HEAD(&free_list, &workers[x], list);

		if(pthread_create(&workers[x].pid, NULL, &run_thread, &workers[x]) < 0) {
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
			fprintf(stderr, "fatal tup error: Graph is not empty after execution. This likely indicates a circular dependency. See the dumped dependency graph for the dependency structure.\n");
			if(fchdir(tup_top_fd()) < 0) {
				perror("fchdir");
			}
			save_graphs(g);
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

static void *run_thread(void *arg)
{
	struct worker_thread *wt = arg;
	struct graph *g = wt->g;
	struct node *n;
	int rc;

	while(1) {
		n = worker_wait(wt);
		if(n == (void*)-1)
			break;
		rc = wt->fn(g, n);
		worker_ret(wt, rc);
	}
	return NULL;
}

static int create_work(struct graph *g, struct node *n)
{
	int rc = 0;
	if(n->tent->type == TUP_NODE_DIR || n->tent->type == TUP_NODE_GHOST) {
		if(tup_entry_variant(n->tent)->enabled) {
			if(n->already_used) {
				rc = 0;
			} else {
				rc = parse(n, g, NULL, refactoring, 1, full_deps);
			}
			show_progress(-1, TUP_NODE_DIR);
		}
	} else if(n->tent->type == TUP_NODE_VAR ||
		  n->tent->type == TUP_NODE_FILE ||
		  n->tent->type == TUP_NODE_GENERATED ||
		  n->tent->type == TUP_NODE_GROUP ||
		  n->tent->type == TUP_NODE_GENERATED_DIR ||
		  n->tent->type == TUP_NODE_CMD) {
		rc = 0;
	} else {
		fprintf(stderr, "tup error: Unknown node type %i with ID %lli named '%s' in create graph.\n", n->tent->type, n->tnode.tupid, n->tent->name.s);
		rc = -1;
	}
	if(tup_db_unflag_create(n->tnode.tupid) < 0)
		rc = -1;

	return rc;
}

static int modify_outputs(struct node *n)
{
	struct edge *e;
	int rc = 0;

	LIST_FOREACH(e, &n->edges, list) {
		if(e->style & TUP_LINK_NORMAL) {
			if(e->dest->tent->type == TUP_NODE_CMD) {
				if(tup_db_add_modify_list(e->dest->tnode.tupid) < 0)
					rc = -1;
			}
			e->dest->skip = 0;
		}
	}
	return rc;
}

static int update_work(struct graph *g, struct node *n)
{
	static int jobs_active = 0;

	struct edge *e;
	int rc = 0;
	if(g) {/* unused */}

	if(n->tent->type == TUP_NODE_CMD) {
		if(!n->skip) {
			pthread_mutex_lock(&display_mutex);
			jobs_active++;
			show_progress(jobs_active, TUP_NODE_CMD);
			pthread_mutex_unlock(&display_mutex);

			rc = update(n);

			pthread_mutex_lock(&display_mutex);
			jobs_active--;
			show_progress(jobs_active, TUP_NODE_CMD);
			pthread_mutex_unlock(&display_mutex);
		} else {
			pthread_mutex_lock(&display_mutex);
			log_debug_tent("Skip cmd", n->tent, "\n");
			skip_result(n->tent);
			show_progress(jobs_active, TUP_NODE_CMD);
			pthread_mutex_unlock(&display_mutex);
		}

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
				if(!e->dest->skip) {
					if(modify_outputs(e->dest) < 0)
						rc = -1;
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
		if(!n->skip) {
			if(modify_outputs(n) < 0)
				rc = -1;
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

	return rc;
}

static int generate_work(struct graph *g, struct node *n)
{
	int rc = 0;
	char *expanded_name = NULL;
	struct tent_entries sticky_root = {NULL};
	struct tent_entries normal_root = {NULL};
	struct tent_entries group_sticky_root = {NULL};
	if(g) {/* unused */}

	if(n->tent->type == TUP_NODE_CMD) {
		const char *cmd;
		if(generate_cwd != n->tent->parent) {
			fprintf(generate_f, "cd \"");
			if(get_relative_dir(generate_f, NULL, generate_cwd->tnode.tupid, n->tent->dt) < 0) {
				rc = -1;
			} else {
				fprintf(generate_f, "\"\n");
				generate_cwd = n->tent->parent;
			}
		}
		cmd = n->tent->name.s;
		rc = tup_db_get_inputs(n->tent->tnode.tupid, &sticky_root, &normal_root, &group_sticky_root);
		if (rc == 0) {
			if(expand_command(&expanded_name, n->tent, cmd, &group_sticky_root, NULL) < 0) {
				fprintf(stderr, "tup error: Failed to expand command '%s' for generate script.\n", n->tent->name.s);
				rc = -1;
			}
			if(expanded_name)
				cmd = expanded_name;
		}
		free_tent_tree(&sticky_root);
		free_tent_tree(&normal_root);
		free_tent_tree(&group_sticky_root);
		if(cmd)
			fprintf(generate_f, "%s\n", cmd);
		free(expanded_name);
	} else {
		rc = 0;
	}

	return rc;
}

static int todo_work(struct graph *g, struct node *n)
{
	/* TUP_NODE_ROOT means we count everything */
	if(n->tent->type == g->count_flags || g->count_flags == TUP_NODE_ROOT) {
		show_result(n->tent, 0, NULL, NULL, 1);
	}

	return 0;
}

static int skip_output(struct tup_entry *tent)
{
	if(tent->type == TUP_NODE_GROUP)
		return 1;
	if(is_virtual_tent(tent->parent))
		return 1;
	return 0;
}

static int move_outputs(struct node *n)
{
	struct edge *e;
	struct node *output;
	char curpath[PATH_MAX];
	char tmppath[PATH_MAX];

	LIST_FOREACH(e, &n->edges, list) {
		output = e->dest;
		if(!skip_output(output->tent)) {
			int output_dfd;
			/* TODO: This is only required to create generated
			 * directories. This should probably be moved
			 * somewhere else.
			 */
			output_dfd = tup_entry_open(output->tent->parent);
			if(output_dfd < 0) {
				fprintf(stderr, "tup error: Unable to open directory to rename previous output files: ");
				print_tup_entry(stderr, output->tent->parent);
				fprintf(stderr, "\n");
				return -1;
			}
			close(output_dfd);

			curpath[0] = '.';
			if(snprint_tup_entry(curpath+1, sizeof(curpath)-1, output->tent) >= (int)sizeof(curpath)-1) {
				fprintf(stderr, "tup error: curpath sized incorrectly in move_outputs()\n");
				return -1;
			}
			if(snprintf(tmppath, sizeof(tmppath), ".tup/tmp/backup-%lli", output->tent->tnode.tupid) >= (int)sizeof(tmppath)) {
				fprintf(stderr, "tup error: tmppath sized incorrectly in move_outputs()\n");
				return -1;
			}
			if(fchdir(tup_top_fd()) < 0)
				return -1;
			if(rename(curpath, tmppath) < 0) {
				/* ENOENT is ok, since the file may not exist
				 * yet (first time we run the command, for
				 * example).
				 */
				if(errno != ENOENT) {
					perror(tmppath);
					fprintf(stderr, "tup error: Unable to move output file '%s' to temporary location '%s'\n", curpath, tmppath);
					return -1;
				}
			}
		}
	}
	return 0;
}

static int restore_outputs(struct node *n)
{
	struct edge *e;
	struct node *output;
	char curpath[PATH_MAX];
	char tmppath[PATH_MAX];

	LIST_FOREACH(e, &n->edges, list) {
		output = e->dest;
		if(!skip_output(output->tent)) {
			curpath[0] = '.';
			if(snprint_tup_entry(curpath+1, sizeof(curpath)-1, output->tent) >= (int)sizeof(curpath)-1) {
				fprintf(stderr, "tup error: curpath sized incorrectly in move_outputs()\n");
				return -1;
			}
			if(snprintf(tmppath, sizeof(tmppath), ".tup/tmp/backup-%lli", output->tent->tnode.tupid) >= (int)sizeof(tmppath)) {
				fprintf(stderr, "tup error: tmppath sized incorrectly in move_outputs()\n");
				return -1;
			}
			if(rename(tmppath, curpath) < 0) {
				/* ENOENT is ok, since the file may not exist
				 * yet (first time we run the command, for
				 * example).
				 */
				if(errno != ENOENT) {
					perror(curpath);
					fprintf(stderr, "tup error: Unable to move output from temporary location '%s' back to '%s'\n", tmppath, curpath);
					return -1;
				}
			}
		}
	}
	return 0;
}

static int compare_files(const char *path1, const char *path2, int *eq)
{
	int fd1;
	int fd2;
	char b1[4096];
	char b2[4096];

	fd1 = open(path1, O_RDONLY);
	if(fd1 < 0) {
		perror(path1);
		fprintf(stderr, "tup error: Unable to open file for comparison.\n");
		return -1;
	}
	fd2 = open(path2, O_RDONLY);
	if(fd2 < 0) {
		perror(path2);
		fprintf(stderr, "tup error: Unable to open file for comparison.\n");
		return -1;
	}
	do {
		int rc1;
		int rc2;
		rc1 = read(fd1, b1, sizeof(b1));
		if(rc1 < 0) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read from file for comparison.\n");
			return -1;
		}
		rc2 = read(fd2, b2, sizeof(b2));
		if(rc2 < 0) {
			perror("read");
			fprintf(stderr, "tup error: Unable to read from file for comparison.\n");
			return -1;
		}
		if(rc1 != rc2)
			goto out_close;
		if(rc1 == 0)
			break;
		if(memcmp(b1, b2, rc1) != 0)
			goto out_close;
	} while(1);

	*eq = 1;

out_close:
	if(close(fd1) < 0) {
		perror("close(fd1)");
		return -1;
	}
	if(close(fd2) < 0) {
		perror("close(fd2)");
		return -1;
	}
	return 0;
}

static int compare_links(const char *path1, const char *path2, int *eq)
{
	char b1[4096];
	char b2[4096];
	int rc1;
	int rc2;

	rc1 = readlinkat(tup_top_fd(), path1, b1, sizeof(b1));
	if(rc1 < 0) {
		perror("readlinkat");
		fprintf(stderr, "tup error: Unable to call readlinkat on path %s\n", path1);
		return -1;
	}
	rc2 = readlinkat(tup_top_fd(), path2, b2, sizeof(b2));
	if(rc2 < 0) {
		perror("readlinkat");
		fprintf(stderr, "tup error: Unable to call readlinkat on path %s\n", path2);
		return -1;
	}
	if(rc1 != rc2)
		return 0;
	if(memcmp(b1, b2, rc1) != 0)
		return 0;
	*eq = 1;
	return 0;
}

static int compare_paths(const char *path1, const char *path2, int *eq)
{
	struct stat buf1;
	struct stat buf2;

	*eq = 0;
	if(lstat(path1, &buf1) < 0) {
		if(errno == ENOENT)
			return 0;
		perror(path1);
		fprintf(stderr, "tup error: Unable to lstat() for comparison.\n");
		return -1;
	}
	if(lstat(path2, &buf2) < 0) {
		if(errno == ENOENT)
			return 0;
		perror(path2);
		fprintf(stderr, "tup error: Unable to lstat() for comparison.\n");
		return -1;
	}
	if(buf1.st_size != buf2.st_size)
		return 0;
	if(buf1.st_mode != buf2.st_mode)
		return 0;

	if(S_ISLNK(buf1.st_mode)) {
		return compare_links(path1, path2, eq);
	} else {
		return compare_files(path1, path2, eq);
	}
}

static int unskip_outputs(struct node *n)
{
	struct edge *e;
	struct node *output;

	LIST_FOREACH(e, &n->edges, list) {
		output = e->dest;
		output->skip = 0;
	}
	return 0;
}

static int check_outputs(struct node *n)
{
	struct edge *e;
	struct node *output;
	char curpath[PATH_MAX];
	char tmppath[PATH_MAX];

	LIST_FOREACH(e, &n->edges, list) {
		output = e->dest;
		if(!skip_output(output->tent)) {
			int eq = 0;

			curpath[0] = '.';
			if(snprint_tup_entry(curpath+1, sizeof(curpath)-1, output->tent) >= (int)sizeof(curpath)-1) {
				fprintf(stderr, "tup error: curpath sized incorrectly in move_outputs()\n");
				return -1;
			}
			if(snprintf(tmppath, sizeof(tmppath), ".tup/tmp/backup-%lli", output->tent->tnode.tupid) >= (int)sizeof(tmppath)) {
				fprintf(stderr, "tup error: tmppath sized incorrectly in move_outputs()\n");
				return -1;
			}
			if(compare_paths(tmppath, curpath, &eq) < 0)
				return -1;
			if(eq) {
				log_debug_tent("Skip file", output->tent, "\n");
			} else {
				output->skip = 0;
			}
		}
	}
	return 0;
}

static int unlink_outputs(int dfd, struct node *n)
{
	struct edge *e;
	struct node *output;
	LIST_FOREACH(e, &n->edges, list) {
		output = e->dest;
		if(!skip_output(output->tent)) {
			int output_dfd = dfd;
			output->skip = 0;
			if(output->tent->dt != n->tent->dt) {
				output_dfd = tup_entry_open(output->tent->parent);
				if(output_dfd < 0) {
					fprintf(stderr, "tup error: Unable to open directory to unlink previous output files: ");
					print_tup_entry(stderr, output->tent->parent);
					fprintf(stderr, "\n");
					return -1;
				}
			}
			if(unlinkat(output_dfd, output->tent->name.s, 0) < 0) {
				if(errno != ENOENT) {
					pthread_mutex_lock(&display_mutex);
					show_result(n->tent, 1, NULL, NULL, 1);
					perror("unlinkat");
					fprintf(stderr, "tup error: Unable to unlink previous output file: %s\n", output->tent->name.s);
					pthread_mutex_unlock(&display_mutex);
					return -1;
				}
			}
			if(output->tent->dt != n->tent->dt) {
				if(close(output_dfd) < 0) {
					perror("close(output_dfd)");
					return -1;
				}
			}
		}
	}
	return 0;
}

static int process_output(struct server *s, struct node *n,
			  struct timespan *ts,
			  const char *expanded_name,
			  int compare_outputs)
{
	FILE *f;
	int is_err = 1;
	struct timespan *show_ts = NULL;
	time_t ms = -1;
	struct tup_entry *tent = n->tent;
	int *warning_dest;
	int important_link_removed = 0;
	int always_display;

	if(show_warnings)
		warning_dest = &warnings;
	else
		warning_dest = NULL;

	f = tmpfile();
	if(!f) {
		show_result(tent, 1, NULL, NULL, 1);
		perror("tmpfile");
		fprintf(stderr, "tup error: Unable to open the error log for writing.\n");
		return -1;
	}
	if(s->exited) {
		if(s->exit_status == 0) {
			if(write_files(f, tent->tnode.tupid, &s->finfo, warning_dest, CHECK_SUCCESS, full_deps, tup_entry_vardt(tent), &important_link_removed) == 0) {
				timespan_end(ts);
				show_ts = ts;
				ms = timespan_milliseconds(ts);

				/* Hooray! */
				is_err = 0;
			}
		} else {
			fprintf(f, " *** Command ID=%lli failed with return value %i\n", tent->tnode.tupid, s->exit_status);
			/* Call write_files just to check for dependency issues */
			write_files(f, tent->tnode.tupid, &s->finfo, warning_dest, CHECK_CMDFAIL, full_deps, tup_entry_vardt(tent), &important_link_removed);
		}
	} else if(s->signalled) {
		int sig = s->exit_sig;
		const char *errmsg = "Unknown signal";

		if(sig >= 0 && sig < ARRAY_SIZE(signal_err) && signal_err[sig])
			errmsg = signal_err[sig];
		fprintf(f, " *** Command ID=%lli killed by signal %i (%s)\n", tent->tnode.tupid, sig, errmsg);
		/* Call write_files just to check for dependency issues */
		write_files(f, tent->tnode.tupid, &s->finfo, warning_dest, CHECK_SIGNALLED, full_deps, tup_entry_vardt(tent), &important_link_removed);
	} else {
		fprintf(f, "tup internal error: Expected s->exited or s->signalled to be set for command ID=%lli", tent->tnode.tupid);
	}

	if(compare_outputs) {
		if(is_err) {
			if(restore_outputs(n) < 0)
				return -1;
		} else {
			if(important_link_removed) {
				if(unskip_outputs(n) < 0)
					return -1;
			} else {
				if(check_outputs(n) < 0)
					return -1;
			}
		}
	}

	fflush(f);
	always_display = 0;
	/* If there are any tup messages, always display the banner. */
	if(ftell(f) > 0) {
		always_display = 1;
	}

	rewind(f);
	if(s->output_fd >= 0) {
		/* If there's any output, always display the banner. */
		if(lseek(s->output_fd, 0, SEEK_END))
			always_display = 1;
		lseek(s->output_fd, 0, SEEK_SET);
	}

	show_result(tent, is_err, show_ts, NULL, always_display);
	if(expanded_name && (is_err || verbose)) {
		FILE *eout = stdout;
		if(is_err)
			eout = stderr;
		fprintf(eout, "tup: Expanded command string: %s\n", expanded_name);
	}
	if(s->output_fd >= 0) {
		if(display_output(s->output_fd, is_err ? 3 : 0, tent->name.s, 0, NULL) < 0)
			return -1;
		if(close(s->output_fd) < 0) {
			perror("close(s->output_fd)");
			return -1;
		}
	}
	if(display_output(fileno(f), 2, tent->name.s, 0, NULL) < 0)
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

#define TMPFILESIZE 32

struct expand_info {
	struct tup_entry *tent;
	const char *groupname;
	int grouplen;
	struct tent_entries *group_sticky_root;
	struct tent_entries *used_groups_root;
};

static int expand_group(FILE *f, struct estring *e, struct expand_info *info)
{
	int group_found = 0;
	int first = 1;
	struct tent_tree *tt;

	RB_FOREACH(tt, tent_entries, info->group_sticky_root) {
		struct tup_entry *group_tent = tt->tent;

		if(memcmp(group_tent->name.s, info->groupname, info->grouplen) == 0) {
			struct tent_entries inputs = {NULL};
			struct tent_tree *ttinput;

			if(info->used_groups_root)
				if(tent_tree_add_dup(info->used_groups_root, group_tent) < 0)
					return -1;
			if(tup_db_get_inputs(group_tent->tnode.tupid, NULL, &inputs, NULL) < 0)
				return -1;
			RB_FOREACH(ttinput, tent_entries, &inputs) {
				struct tup_entry *input_tent = ttinput->tent;
				if(input_tent->type == TUP_NODE_GENERATED) {
					if(e && !first)
						if(estring_append(e, " ", 1) < 0)
							return -1;
					if(get_relative_dir(f, e, info->tent->parent->tnode.tupid, input_tent->tnode.tupid) < 0)
						return -1;
					if(f)
						fprintf(f, "\n");
					first = 0;
				}
			}
			free_tent_tree(&inputs);
			group_found = 1;
		}
	}
	if(!group_found) {
		fprintf(stderr, "tup error: Unable to find group '%.*s' as an input for use as a resource file. Make sure it is listed as an input to the command: ", info->grouplen, info->groupname);
		print_tup_entry(stderr, info->tent);
		fprintf(stderr, "\n");
		return -1;
	}
	return 0;
}

static int expand_group_inline(struct estring *expanded_name,
			       struct expand_info *info)
{
	if(expand_group(NULL, expanded_name, info) < 0)
		return -1;
	return 0;
}

static int expand_res_file(struct estring *expanded_name,
			   struct expand_info *info)
{
	static int resfile = 0;
	FILE *f;
	char tmpfilename[TMPFILESIZE];
	int tmpfilenamelen;
	int x;
	int num_dotdots = 0;
	struct tup_entry *tmp;

	tmp = info->tent->parent;
	while(tmp->parent) {
		num_dotdots++;
		tmp = tmp->parent;
	}

	snprintf(tmpfilename, TMPFILESIZE, ".tup/tmp/res-%i", resfile);
	resfile++;
	/* Use binary so newlines aren't converted on Windows.
	 * Both cl and cygwin can handle UNIX line-endings, but
	 * cygwin barfs on Windows line-endings.
	 */
	f = fopen(tmpfilename, "wb");
	if(!f) {
		perror(tmpfilename);
		fprintf(stderr, "tup error: Unable to create temporary resource file.\n");
		return -1;
	}
	if(expand_group(f, NULL, info) < 0)
		return -1;
	fclose(f);
	tmpfilename[TMPFILESIZE-1] = 0;
	tmpfilenamelen = strlen(tmpfilename);

	for(x=0; x<num_dotdots; x++) {
		if(estring_append(expanded_name, "../", 3) < 0)
			return -1;
	}
	if(estring_append(expanded_name, tmpfilename, tmpfilenamelen) < 0)
		return -1;
	return 0;
}

static int expand_command(char **res,
			  struct tup_entry *tent, const char *cmd,
			  struct tent_entries *group_sticky_root,
			  struct tent_entries *used_groups_root)
{
	struct estring expanded_name;
	const char *percgroup;
	const char *tcmd;
	int tcmdlen;
	struct expand_info info = {
		.tent = tent,
		.groupname = NULL,
		.grouplen = 0,
		.group_sticky_root = group_sticky_root,
		.used_groups_root = used_groups_root,
	};

	if(strstr(cmd, "%<") == NULL) {
		*res = NULL;
		return 0;
	}

	if(estring_init(&expanded_name) < 0)
		return -1;
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		fprintf(stderr, "tup error: Unable to create temporary resource file.\n");
		return -1;
	}

	tcmd = cmd;
	while((percgroup = strstr(tcmd, "%<")) != NULL) {
		int prelen = percgroup - tcmd;
		char *endgroup;

		if(estring_append(&expanded_name, tcmd, prelen) < 0)
			return -1;

		endgroup = strchr(percgroup, '>');
		if(!endgroup) {
			fprintf(stderr, "tup error: Unable to find end-group marker '>' for %%<group> flag.\n");
			return -1;
		}
		endgroup++;

		info.groupname = percgroup + 1;
		info.grouplen = endgroup - info.groupname;
		tcmd = endgroup;
		if(strncmp(tcmd, ".res", 4) == 0) {
			tcmd += 4;
			if(expand_res_file(&expanded_name, &info) < 0)
				return -1;
		} else {
			if(expand_group_inline(&expanded_name, &info) < 0)
				return -1;
		}
	}
	tcmdlen = strlen(tcmd);
	if(estring_append(&expanded_name, tcmd, tcmdlen) < 0)
		return -1;
	if(estring_append(&expanded_name, "\0", 1) < 0)
		return -1;
	*res = expanded_name.s;
	return 0;
}

static const char *input_and_output_from_cmd(const char *cmd, struct tup_entry *dtent, char *input, char *output)
{
	const char *endp;
	size_t input_len;
	while(isspace(*cmd))
		cmd++;
	endp = cmd;
	while(!isspace(*endp))
		endp++;
	input_len = endp - cmd;
	if(input_len >= PATH_MAX - 1)
		return NULL;
	strncpy(input, cmd, input_len);
	input[input_len] = 0;
	while(isspace(*endp))
		endp++;
	if(is_full_path(endp)) {
		strcpy(output, endp);
	} else {
		output[0] = '.';
		snprint_tup_entry(output+1, PATH_MAX-1, dtent);
		strcat(output, "/");
		strcat(output, endp);
	}
	return endp;
}

static int specify_pseudo_exec_output(struct server *s, const char *output)
{
	struct mapping *map;
	if(handle_file(ACCESS_WRITE, output, NULL, &s->finfo) < 0)
		return -1;
	map = malloc(sizeof *map);
	if(!map) {
		perror("malloc");
		return -1;
	}
	map->realname = strdup(output);
	map->tmpname = strdup(output);
	map->tent = NULL;
	finfo_lock(&s->finfo);
	LIST_INSERT_HEAD(&s->finfo.mapping_list, map, list);
	finfo_unlock(&s->finfo);

	s->exited = 1;
	s->exit_status = 0;
	return 0;
}

static int do_ln(struct server *s, struct tup_entry *dtent, int dfd, const char *cmd)
{
	char input_path[PATH_MAX];
	char full_output_path[PATH_MAX];
	const char* output_name = input_and_output_from_cmd(cmd, dtent, input_path, full_output_path);
	if(!output_name)
		return -1;
	if(server_symlink(s, input_path, dfd, output_name) < 0)
		return -1;
	return specify_pseudo_exec_output(s, full_output_path);
}

static int update(struct node *n)
{
	int dfd = -1;
	char *expanded_name = NULL;
	const char *cmd;
	struct server s;
	int rc;
	struct tup_env newenv;
	struct timespan ts;
	int need_namespacing = 0;
	int compare_outputs = 0;
	int run_in_bash = 0;
	int use_server = 0;

	timespan_start(&ts);
	if(n->tent->flags) {
		int x;
		for(x=0; x<n->tent->flagslen; x++) {
			switch(n->tent->flags[x]) {
				case 'c':
					need_namespacing = 1;
					break;
				case 'o':
					compare_outputs = 1;
					break;
				case 'b':
					run_in_bash = 1;
					break;
				default:
					pthread_mutex_lock(&display_mutex);
					show_result(n->tent, 1, NULL, NULL, 1);
					fprintf(stderr, "tup error: Unknown ^-flag: '%c'\n", n->tent->flags[x]);
					pthread_mutex_unlock(&display_mutex);
					return -1;
			}
		}
	}
	cmd = n->tent->name.s;

	dfd = tup_entry_open(n->tent->parent);
	if(dfd < 0) {
		pthread_mutex_lock(&display_mutex);
		show_result(n->tent, 1, NULL, NULL, 1);
		fprintf(stderr, "tup error: Unable to open directory for update work.\n");
		tup_db_print(stderr, n->tent->parent->tnode.tupid);
		pthread_mutex_unlock(&display_mutex);
		goto err_out;
	}

	if(compare_outputs) {
		if(move_outputs(n) < 0)
			goto err_close_dfd;
	} else {
		if(unlink_outputs(dfd, n) < 0)
			goto err_close_dfd;
	}

	pthread_mutex_lock(&db_mutex);
	rc = initialize_server_struct(&s, n->tent);
	if(rc == 0)
		rc = tup_db_get_environ(&s.finfo.sticky_root, &s.finfo.normal_root, &newenv);
	if(rc == 0) {
		if(expand_command(&expanded_name, n->tent, cmd, &s.finfo.group_sticky_root, &s.finfo.used_groups_root) < 0)
			rc = -1;
		if(expanded_name)
			cmd = expanded_name;
	}
	pthread_mutex_unlock(&db_mutex);
	if(rc < 0)
		goto err_close_dfd;
	if(strncmp(cmd, "!tup_ln ", 8) == 0) {
		pthread_mutex_lock(&db_mutex);
		rc = do_ln(&s, n->tent->parent, dfd, cmd + 8);
		pthread_mutex_unlock(&db_mutex);
	} else if (strncmp(cmd, "!tup_preserve ", 14) == 0) {
		pthread_mutex_lock(&db_mutex);
		rc = do_ln(&s, n->tent->parent, dfd, cmd + 14);
		pthread_mutex_unlock(&db_mutex);
	} else {
		rc = server_exec(&s, dfd, cmd, &newenv, n->tent->parent, need_namespacing, run_in_bash);
		use_server = 1;
	}
	if(rc < 0) {
		pthread_mutex_lock(&display_mutex);
		fprintf(stderr, " *** Command ID=%lli failed: %s\n", n->tnode.tupid, cmd);
		pthread_mutex_unlock(&display_mutex);
		free(expanded_name);
		goto err_close_dfd;
	}
	environ_free(&newenv);
	if(close(dfd) < 0) {
		perror("close(dfd)");
		return -1;
	}

	pthread_mutex_lock(&db_mutex);
	pthread_mutex_lock(&display_mutex);
	rc = process_output(&s, n, &ts, expanded_name, compare_outputs);
	pthread_mutex_unlock(&display_mutex);
	pthread_mutex_unlock(&db_mutex);
	free(expanded_name);
	cleanup_file_info(&s.finfo);
	if(use_server)
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
