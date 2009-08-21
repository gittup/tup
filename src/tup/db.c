#define _ATFILE_SOURCE
#include "db.h"
#include "db_util.h"
#include "array_size.h"
#include "linux/list.h"
#include "tupid_tree.h"
#include "fileio.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include "sqlite3/sqlite3.h"

#define DB_VERSION 11

enum {
	DB_BEGIN,
	DB_COMMIT,
	DB_ROLLBACK,
	DB_CHECK_DUP_LINKS,
	DB_SELECT_DBN_BY_ID,
	DB_SELECT_DIRNAME,
	DB_SELECT_NODE_BY_FLAGS_1,
	DB_SELECT_NODE_BY_FLAGS_2,
	DB_SELECT_NODE_BY_FLAGS_3,
	DB_SELECT_NODE_DIR,
	DB_SELECT_NODE_DIR_GLOB,
	DB_DELETE_NODE,
	DB_MODIFY_DIR,
	DB_OPEN_TUPID,
	DB_PARENT,
	DB_IS_ROOT_NODE,
	DB_CHANGE_NODE_NAME,
	DB_GET_TYPE,
	DB_SET_TYPE,
	DB_SET_SYM,
	DB_SET_MTIME,
	DB_PRINT,
	_DB_NODELIST_LEN,
	_DB_GET_NODELIST,
	DB_ADD_DIR_CREATE_LIST,
	DB_ADD_CREATE_LIST,
	DB_ADD_MODIFY_LIST,
	DB_IN_CREATE_LIST,
	DB_IN_MODIFY_LIST,
	DB_UNFLAG_CREATE,
	DB_UNFLAG_MODIFY,
	_DB_GET_RECURSE_DIRS,
	_DB_GET_DIR_ENTRIES,
	DB_DELETE_EMPTY_LINKS,
	DB_YELL_LINKS,
	DB_LINK_EXISTS,
	DB_LINK_STYLE,
	DB_GET_INCOMING_LINK,
	DB_DELETE_LINKS,
	DB_UNSTICKY_LINKS,
	DB_CMDS_TO_TREE,
	DB_CMD_OUTPUTS_TO_TREE,
	DB_MODIFY_CMDS_BY_OUTPUT,
	DB_MODIFY_CMDS_BY_INPUT,
	DB_SET_DEPENDENT_DIR_FLAGS,
	DB_SELECT_NODE_BY_LINK,
	DB_DELETE_DEPENDENT_DIR_LINKS,
	DB_CONFIG_SET_INT,
	DB_CONFIG_GET_INT,
	DB_CONFIG_SET_INT64,
	DB_CONFIG_GET_INT64,
	DB_CONFIG_SET_STRING,
	DB_CONFIG_GET_STRING,
	DB_SET_VAR,
	_DB_GET_VAR_ID,
	DB_GET_VAR_ID_ALLOC,
	DB_GET_VARLEN,
	DB_WRITE_VAR,
	DB_VAR_FOREACH,
	DB_FILES_TO_TMP,
	DB_UNFLAG_TMP,
	DB_GET_ALL_IN_TMP,
	DB_CLEAR_TMP_LIST,
	DB_ADD_WRITE_LIST,
	DB_NODE_INSERT,
	_DB_NODE_SELECT,
	_DB_LINK_INSERT,
	_DB_LINK_UPDATE,
	_DB_NODE_HAS_GHOSTS,
	_DB_CREATE_GHOST_LIST,
	_DB_CREATE_TMP_LIST,
	_DB_ADD_GHOST_DT_SYM,
	_DB_ADD_GHOST_LINKS,
	_DB_ADD_GHOST_DIRS,
	_DB_RECLAIM_GHOSTS,
	_DB_CLEAR_GHOST_LIST,
	_DB_INIT_VAR_LIST,
	_DB_VAR_LIST_FLAG_DIRS,
	_DB_VAR_LIST_FLAG_CMDS,
	_DB_VAR_LIST_UNFLAG_CREATE,
	_DB_VAR_LIST_UNFLAG_MODIFY,
	_DB_VAR_LIST_DELETE_LINKS,
	_DB_VAR_LIST_DELETE_VARS,
	_DB_VAR_LIST_DELETE_NODES,
	_DB_CHECK_EXPECTED_OUTPUTS,
	_DB_CHECK_ACTUAL_OUTPUTS,
	_DB_CHECK_ACTUAL_INPUTS,
	_DB_ADD_INPUT_LINKS,
	_DB_STYLE_INPUT_LINKS,
	_DB_DROP_OLD_LINKS,
	_DB_UNSTYLE_OLD_LINKS,
	_DB_ADD_OUTPUTS,
	_DB_REMOVE_OUTPUTS,
	DB_NUM_STATEMENTS
};

struct var_list {
	struct list_head vars;
	unsigned int count;
};

struct var_entry {
	struct list_head list;
	const char *var;
	const char *value;
	int varlen;
	int vallen;
};

static sqlite3 *tup_db = NULL;
static sqlite3_stmt *stmts[DB_NUM_STATEMENTS];
static int tup_db_var_changed = 0;
static int num_ghosts = 0;
static int sql_debug = 0;

static int version_check(void);
static int node_select(tupid_t dt, const char *name, int len,
		       struct db_node *dbn);

static int link_insert(tupid_t a, tupid_t b, int style);
static int link_update(tupid_t a, tupid_t b, int style);
static int node_has_ghosts(tupid_t tupid);
static int create_ghost_list(void);
static int create_tmp_list(void);
static int check_tmp_requested(void);
static int add_ghost_dt_sym(tupid_t tupid);
static int add_ghost_links(tupid_t tupid);
static int add_ghost_dirs(void);
static int clear_ghost_list(void);
static int reclaim_ghosts(void);
static int init_var_list(void);
static int var_list_flag_dirs(void);
static int var_list_flag_cmds(void);
static int var_list_unflag_create(void);
static int var_list_unflag_modify(void);
static int var_list_delete_links(void);
static int var_list_delete_vars(void);
static int var_list_delete_nodes(void);
static int check_expected_outputs(tupid_t cmdid);
static int check_actual_outputs(tupid_t cmdid);
static int check_actual_inputs(tupid_t cmdid);
static int add_input_links(tupid_t cmdid);
static int style_input_links(tupid_t cmdid);
static int drop_old_links(tupid_t cmdid);
static int unstyle_old_links(tupid_t cmdid);
static int add_outputs(tupid_t cmdid, int *outputs_differ);
static int remove_outputs(tupid_t cmdid, int *outputs_differ);
static int no_sync(void);
static int generated_nodelist_len(tupid_t dt);
static int get_generated_nodelist(char *dest, tupid_t dt, struct rb_root *tree);
static int db_print(FILE *stream, tupid_t tupid);
static int get_recurse_dirs(tupid_t dt, struct list_head *list);
static int get_dir_entries(tupid_t dt, struct list_head *list);

int tup_db_open(void)
{
	int rc;
	int x;

	rc = sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != 0) {
		fprintf(stderr, "Unable to open database: %s\n",
			sqlite3_errmsg(tup_db));
	}
	for(x=0; x<ARRAY_SIZE(stmts); x++) {
		stmts[x] = NULL;
	}

	if(tup_db_config_get_int("db_sync") == 0) {
		if(no_sync() < 0)
			return -1;
	}
	if(version_check() < 0)
		return -1;

	if(create_ghost_list() < 0)
		return -1;
	if(create_tmp_list() < 0)
		return -1;

	return rc;
}

int tup_db_close(void)
{
	return db_close(tup_db, stmts, ARRAY_SIZE(stmts));
}

int tup_db_create(int db_sync)
{
	int rc;
	int x;
	const char *sql[] = {
		"create table node (id integer primary key not null, dir integer not null, type integer not null, sym integer not null, mtime integer not null, name varchar(4096))",
		"create table link (from_id integer, to_id integer, style integer, unique(from_id, to_id))",
		"create table var (id integer primary key not null, value varchar(4096))",
		"create table config(lval varchar(256) unique, rval varchar(256))",
		"create table create_list (id integer primary key not null)",
		"create table modify_list (id integer primary key not null)",
		"create index node_dir_index on node(dir, name)",
		"create index node_sym_index on node(sym)",
		"create index link_index2 on link(to_id)",
		"insert into config values('show_progress', 1)",
		"insert into config values('keep_going', 0)",
		"insert into config values('db_sync', 1)",
		"insert into config values('db_version', 0)",
		"insert into config values('autoupdate', 0)",
		"insert into config values('num_jobs', 1)",
		"insert into node values(1, 0, 2, -1, -1, '.')",
		"insert into node values(2, 1, 2, -1, -1, '@')",
	};

	rc = sqlite3_open(TUP_DB_FILE, &tup_db);
	if(rc == 0) {
		printf(".tup repository initialized.\n");
	} else {
		fprintf(stderr, "Unable to create database: %s\n",
			sqlite3_errmsg(tup_db));
	}

	if(db_sync == 0) {
		if(no_sync() < 0)
			return -1;
	}

	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(tup_db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s",
				errmsg, sql[x]);
			return -1;
		}
	}

	if(db_sync == 0) {
		if(tup_db_config_set_int("db_sync", 0) < 0)
			return -1;
	}
	if(tup_db_config_set_int("db_version", DB_VERSION) < 0)
		return -1;

	return 0;
}

static int version_check(void)
{
	int version;
	char *errmsg;
	char sql_1a[] = "alter table link add column style integer default 0";
	char sql_1b[] = "insert or replace into create_list select id from node where type=2 and not id=2";
	char sql_1c[] = "update node set type=4 where id in (select to_id from link) and type=0";

	char sql_2a[] = "update link set style=2 where style=1";
	char sql_2b[] = "update link set style=1 where style=0";
	char sql_2c[] = "insert into link select from_id, to_id, sum(style) from link group by from_id, to_id";
	char sql_2d[] = "delete from link where rowid not in (select rowid from link group by from_id, to_id having max(style))";

	char sql_3a[] = "alter table node add column sym integer default -1";

	char sql_4a[] = "drop index link_index";
	char sql_4b[] = "create index link_index on link(from_id, to_id)";

	char sql_5a[] = "create index node_sym_index on node(sym)";

	char sql_6a[] = "create table ghost_list (id integer primary key not null)";

	char sql_7a[] = "drop table ghost_list";

	char sql_8a[] = "alter table node add column mtime integer default -1";

	char sql_9a[] = "create table link_new (from_id integer, to_id integer, style integer, unique(from_id, to_id))";
	char sql_9b[] = "insert or ignore into link_new select from_id, to_id, style from link";
	char sql_9c[] = "drop index link_index";
	char sql_9d[] = "drop index link_index2";
	char sql_9e[] = "drop table link";
	char sql_9f[] = "alter table link_new rename to link";
	char sql_9g[] = "create index link_index2 on link(to_id)";

	char sql_10[] = "drop table delete_list";

	version = tup_db_config_get_int("db_version");
	if(version < 0) {
		fprintf(stderr, "Error getting .tup/db version.\n");
		return -1;
	}

	switch(version) {
		case 1:
			if(sqlite3_exec(tup_db, sql_1a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_1a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_1b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_1b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_1c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_1c);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 2) < 0)
				return -1;
			fprintf(stderr, "WARNING: Tup database updated to version 2.\nThe link table has a new column (style) to annotate the origin of the link. This is used to differentiate between links specified in Tupfiles vs. links determined automatically via wrapped command execution, so the links can be removed at appropriate times. Also, a new node type (TUP_NODE_GENERATED==4) has been added. All files created from commands have been updated to this new type. This is used so you can't try to create a command to write to a base source file. All Tupfiles will be re-parsed on the next update in order to generate the new links. If you have any problems, it might be easiest to re-checkout your code and start anew. Admittedly I haven't tested the conversion completely.\n");

			fprintf(stderr, "NOTE: If you are using the file monitor, you probably want to restart it.\n");
		case 2:
			if(sqlite3_exec(tup_db, sql_2a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_2a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_2b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_2b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_2c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_2c);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_2d, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_2d);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 3) < 0)
				return -1;
			fprintf(stderr, "WARNING: Tup database updated to version 3.\nThe style column in the link table now uses flags instead of multiple records. For example, a link from ID 5 to 7 used to contain 5|7|0 for a normal link and 5|7|1 for a sticky link. Now it is 5|7|1 for a normal link, 5|7|2 for a sticky link, and 5|7|3 for both links.\n");
		case 3:
			if(sqlite3_exec(tup_db, sql_3a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_3a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 4) < 0)
				return -1;
			fprintf(stderr, "WARNING: Tup database updated to version 4.\nA 'sym' column has been added to the node table so symlinks can reference their destination nodes. This is necessary in order to properly handle dependencies on symlinks in an efficient manner.\nWARNING: If you have any symlinks in your system, you probably want to delete and re-create them with the monitor running.\n");
		case 4:
			if(sqlite3_exec(tup_db, sql_4a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_4a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_4b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_4b);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 5) < 0)
				return -1;
			fprintf(stderr, "NOTE: Tup database updated to version 5.\nThis is a pretty minor update - the link_index is adjusted to use (from_id, to_id) instead of just (from_id). This greatly improves the performance of link insertion, since a query has to be done for uniqueness and style constraints.\n");

		case 5:
			if(sqlite3_exec(tup_db, sql_5a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_5a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 6) < 0)
				return -1;
			fprintf(stderr, "NOTE: Tup database updated to version 6.\nAnother minor update - just adding an index on node.sym so it can be quickly determined if a deleted node needs to be made into a ghost.\n");

		case 6:
			if(sqlite3_exec(tup_db, sql_6a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_6a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 7) < 0)
				return -1;
			fprintf(stderr, "NOTE: Tup database updated to version 7.\nThis includes a ghost_list for storing ghost ids so they can later be raptured.\n");

		case 7:
			if(sqlite3_exec(tup_db, sql_7a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_7a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 8) < 0)
				return -1;
			fprintf(stderr, "NOTE: Tup database updated to version 8.\nThis is really the same as version 6. Turns out putting the ghost_list on disk was kinda stupid. Now it's all handled in a temporary table in memory during a transaction.\n");

		case 8:
			if(sqlite3_exec(tup_db, sql_8a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_8a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 9) < 0)
				return -1;
			fprintf(stderr, "WARNING: Tup database updated to version 9.\nThis version includes a per-file timestamp in order to determine if a file has changed in between monitor invocations, or during a scan. You will want to restart the monitor in order to set the mtime field for all the files. Note that since no mtimes currently exist in the database, this will cause all commands to be executed for the next update.\n");

		case 9:
			if(sqlite3_exec(tup_db, sql_9a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9c);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9d, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9d);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9e, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9e);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9f, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9f);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9g, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_9g);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 10) < 0)
				return -1;
			fprintf(stderr, "NOTE: Tup database updated to version 10.\nA new unique constraint was placed on the link table.\n");

		case 10:
			if(sqlite3_exec(tup_db, sql_10, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s",
					errmsg, sql_10);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 11) < 0)
				return -1;
			fprintf(stderr, "NOTE: This database goes to 11.\nThe delete_list is no longer necessary, and is now gone.\n");

		case DB_VERSION:
			break;
		default:
			fprintf(stderr, "Error: Database version %i not compatible with %i\n", version, DB_VERSION);
			return -1;
	}

	return 0;
}

int tup_db_begin(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_BEGIN];
	static char s[] = "begin";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	return 0;
}

int tup_db_commit(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_COMMIT];
	static char s[] = "commit";

	if(reclaim_ghosts() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	return 0;
}

int tup_db_rollback(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ROLLBACK];
	static char s[] = "rollback";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	return 0;
}

static const char *check_flags_name;
static int check_flags_cb(void *arg, int argc, char **argv, char **col)
{
	int *iptr = arg;
	if(argc) {}
	if(argv) {}
	if(col) {}
	if(check_flags_name) {
		fprintf(stderr, "*** %s_list:\n", check_flags_name);
		check_flags_name = NULL;
	}
	fprintf(stderr, "%s\n", argv[0]);
	*iptr = 1;
	return 0;
}

int tup_db_check_flags(void)
{
	int rc = 0;
	char *errmsg;
	char s1[] = "select * from create_list";
	char s2[] = "select * from modify_list";

	if(sql_debug) fprintf(stderr, "%s\n", s1);
	check_flags_name = "create";
	if(sqlite3_exec(tup_db, s1, check_flags_cb, &rc, &errmsg) != 0) {
		fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
			errmsg, s1);
		sqlite3_free(errmsg);
		return -1;
	}
	if(sql_debug) fprintf(stderr, "%s\n", s2);
	check_flags_name = "modify";
	if(sqlite3_exec(tup_db, s2, check_flags_cb, &rc, &errmsg) != 0) {
		fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
			errmsg, s2);
		sqlite3_free(errmsg);
		return -1;
	}
	return rc;
}

int tup_db_check_dup_links(void)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_CHECK_DUP_LINKS];
	static char s[] = "select from_id, to_id, count(from_id) from link group by from_id, to_id having count(from_id) > 1";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	while(1) {
		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		fprintf(stderr, "tup error: Duplicate link %lli -> %lli exists %i times\n", sqlite3_column_int64(*stmt, 0), sqlite3_column_int64(*stmt, 1), sqlite3_column_int(*stmt, 2));
		rc = -1;
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

void tup_db_enable_sql_debug(void)
{
	sql_debug = 1;
}

tupid_t tup_db_create_node(tupid_t dt, const char *name, int type)
{
	return tup_db_create_node_part(dt, name, -1, type);
}

tupid_t tup_db_create_node_part(tupid_t dt, const char *name, int len, int type)
{
	struct db_node dbn;
	tupid_t tupid;

	if(node_select(dt, name, len, &dbn) < 0) {
		return -1;
	}

	if(dbn.tupid != -1) {
		if(dbn.type == TUP_NODE_GHOST) {
			if(tup_db_set_type(dbn.tupid, type) < 0)
				return -1;
			return dbn.tupid;
		}
		if(dbn.type != type) {
			/* Try to provide a more sane error message in this
			 * case, since a user might come across it just by
			 * screwing up the Tupfile.
			 */
			if(dbn.type == TUP_NODE_FILE && type == TUP_NODE_GENERATED) {
				fprintf(stderr, "Error: Attempting to insert '%s' as a generated node when it already exists as a user file. You can do one of two things to fix this:\n  1) If this file is really supposed to be created from the command, delete the file from the filesystem and try again.\n  2) Change your rule in the Tupfile so you aren't trying to overwrite the file.\nThis error message is brought to you by the Defenders of the Truth to prevent   you from doing stupid things like delete your source code.\n", name);
				return -1;
			}
			fprintf(stderr, "tup error: Attempting to insert node '%s' with type %i, which already exists as type %i\n", name, type, dbn.type);
			return -1;
		}
		return dbn.tupid;
	}

	tupid = tup_db_node_insert(dt, name, len, type, -1);
	if(tupid < 0)
		return -1;
	return tupid;
}

int tup_db_select_dbn_by_id(tupid_t tupid, struct db_node *dbn)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_DBN_BY_ID];
	static char s[] = "select dir, type, sym from node where id=?";

	dbn->tupid = -1;
	dbn->dt = -1;
	dbn->name = NULL;
	dbn->type = 0;
	dbn->sym = -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = 0;
	dbn->tupid = tupid;
	dbn->dt = sqlite3_column_int64(*stmt, 0);
	dbn->type = sqlite3_column_int64(*stmt, 1);
	dbn->sym = sqlite3_column_int64(*stmt, 2);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

tupid_t tup_db_select_dirname(tupid_t tupid, char **name)
{
	tupid_t dt = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_DIRNAME];
	static char s[] = "select dir, name from node where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	*name = strdup((const char *)sqlite3_column_text(*stmt, 1));
	if(!*name) {
		perror("strdup");
		goto out_reset;
	}
	dt = sqlite3_column_int64(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return dt;
}

int tup_db_select_dbn(tupid_t dt, const char *name, struct db_node *dbn)
{
	if(node_select(dt, name, -1, dbn) < 0)
		return -1;

	dbn->dt = dt;
	dbn->name = name;

	return 0;
}

int tup_db_select_dbn_part(tupid_t dt, const char *name, int len,
			   struct db_node *dbn)
{
	if(node_select(dt, name, len, dbn) < 0)
		return -1;

	dbn->dt = dt;
	dbn->name = name;

	return 0;
}

int tup_db_select_node_by_flags(int (*callback)(void *, struct db_node *,
						int style),
				void *arg, int flags)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt;
	static char s1[] = "select id, dir, name, type, sym, mtime from node where id in (select * from create_list)";
	static char s2[] = "select id, dir, name, type, sym, mtime from node where id in (select * from modify_list)";
	char *sql;
	int sqlsize;

	if(flags == TUP_FLAGS_CREATE) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_1];
		sql = s1;
		sqlsize = sizeof(s1);
	} else if(flags == TUP_FLAGS_MODIFY) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_2];
		sql = s2;
		sqlsize = sizeof(s2);
	} else {
		fprintf(stderr, "Error: tup_db_select_node_by_flags() must specify exactly one of TUP_FLAGS_CREATE/TUP_FLAGS_MODIFY\n");
		return -1;
	}

	if(sql_debug) fprintf(stderr, "%s\n", sql);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, sql, sqlsize, stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), sql);
			return -1;
		}
	}

	while(1) {
		struct db_node dbn;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		dbn.tupid = sqlite3_column_int64(*stmt, 0);
		dbn.dt = sqlite3_column_int64(*stmt, 1);
		dbn.name = (const char *)sqlite3_column_text(*stmt, 2);
		dbn.type = sqlite3_column_int(*stmt, 3);
		dbn.sym = sqlite3_column_int64(*stmt, 4);
		dbn.mtime = sqlite3_column_int64(*stmt, 5);

		/* Since this is used to build the initial part of the DAG,
		 * we use TUP_LINK_NORMAL so the nodes that are returned will
		 * be expanded.
		 */
		if((rc = callback(arg, &dbn, TUP_LINK_NORMAL)) < 0) {
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_select_node_dir(int (*callback)(void *, struct db_node *, int style),
			   void *arg, tupid_t dt)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_DIR];
	static char s[] = "select id, name, type, sym, mtime from node where dir=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, dt);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		dbn.tupid = sqlite3_column_int64(*stmt, 0);
		dbn.dt = dt;
		dbn.name = (const char *)sqlite3_column_text(*stmt, 1);
		dbn.type = sqlite3_column_int(*stmt, 2);
		dbn.sym = sqlite3_column_int64(*stmt, 3);
		dbn.mtime = sqlite3_column_int64(*stmt, 4);

		/* This is used by the 'tup g' function if the user wants to
		 * graph a directory. Since we want to expand all nodes in the
		 * directory, we use TUP_LINK_NORMAL.
		 */
		if(callback(arg, &dbn, TUP_LINK_NORMAL) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_select_node_dir_glob(int (*callback)(void *, struct db_node *),
				void *arg, tupid_t dt, const char *glob)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_DIR_GLOB];
	static char s[] = "select id, name, type, sym, mtime from node where dir=? and (type=? or type=?) and name glob ?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i, %i, '%s'][0m\n", s, dt, TUP_NODE_FILE, TUP_NODE_GENERATED, glob);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_FILE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 4, glob, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		dbn.tupid = sqlite3_column_int64(*stmt, 0);
		dbn.dt = dt;
		dbn.name = (const char *)sqlite3_column_text(*stmt, 1);
		dbn.type = sqlite3_column_int(*stmt, 2);
		dbn.sym = sqlite3_column_int64(*stmt, 3);
		dbn.mtime = sqlite3_column_int64(*stmt, 4);

		if(callback(arg, &dbn) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_delete_node(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_NODE];
	static char s[] = "delete from node where id=?";

	if(add_ghost_dt_sym(tupid) < 0)
		return -1;

	rc = node_has_ghosts(tupid);
	if(rc < 0)
		return -1;
	if(rc == 1) {
		/* We're but a ghost now... make sure we don't point at
		 * anybody (t5033). Ghosts don't have fingers, you know.
		 */
		if(tup_db_set_type(tupid, TUP_NODE_GHOST) < 0)
			return -1;
		if(tup_db_set_sym(tupid, -1) < 0)
			return -1;
		return 0;
	}

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_delete_dir(tupid_t dt)
{
	LIST_HEAD(subdir_list);

	if(get_dir_entries(dt, &subdir_list) < 0)
		return -1;
	while(!list_empty(&subdir_list)) {
		struct id_entry *ide = list_entry(subdir_list.next,
						  struct id_entry, list);
		/* tup_del_id may call back to tup_db_delete_dir() */
		if(tup_del_id(ide->tupid) < 0)
			return -1;
		list_del(&ide->list);
		free(ide);
	}

	return 0;
}

int tup_db_delete_file(tupid_t tupid)
{
	int rc;
	char *name;
	tupid_t dt;

	dt = tup_db_select_dirname(tupid, &name);
	if(dt < 0)
		return -1;
	rc = delete_file(dt, name);
	free(name);
	return rc;
}

int tup_db_modify_dir(tupid_t dt)
{
	LIST_HEAD(subdir_list);
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MODIFY_DIR];
	static char s[] = "insert or replace into modify_list select id from node where dir=? and type!=?";

	if(tup_db_add_create_list(dt) < 0)
		return -1;
	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, dt, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(get_recurse_dirs(dt, &subdir_list) < 0)
		return -1;
	while(!list_empty(&subdir_list)) {
		struct id_entry *ide = list_entry(subdir_list.next,
						  struct id_entry, list);
		tup_db_modify_dir(ide->tupid);
		list_del(&ide->list);
		free(ide);
	}

	return 0;
}

int tup_db_open_tupid(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_OPEN_TUPID];
	static char s[] = "select dir, name from node where id=?";
	tupid_t parent;
	char *path;
	int fd;

	if(tupid == 0) {
		fprintf(stderr, "Error: Trying to tup_db_open_tupid(0)\n");
		return -1;
	}
	if(tupid == 1) {
		return dup(tup_top_fd());
	}
	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -ENOENT;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	parent = sqlite3_column_int64(*stmt, 0);
	path = strdup((const char *)sqlite3_column_text(*stmt, 1));
	if(!path) {
		perror("strdup");
		rc = -1;
		goto out_reset;
	}
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	fd = tup_db_open_tupid(parent);
	if(fd < 0) {
		free(path);
		return fd;
	}

	rc = openat(fd, path, O_RDONLY);
	if(rc < 0) {
		if(errno == ENOENT)
			rc = -ENOENT;
		else
			perror(path);
	}
	close(fd);
	free(path);

	return rc;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

tupid_t tup_db_parent(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_PARENT];
	static char s[] = "select dir from node where id=?";
	tupid_t parent;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc == SQLITE_DONE) {
		parent = -1;
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		parent = -1;
		goto out_reset;
	}

	parent = sqlite3_column_int64(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return parent;
}

int tup_db_is_root_node(tupid_t tupid)
{
	int rc;
	int dbrc;
	int type;
	sqlite3_stmt **stmt = &stmts[DB_IS_ROOT_NODE];
	static char s[] = "select type from node where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr, "tup error: tup_db_is_root_node() called on node (%lli) that doesn't exist?\n", tupid);
		rc = -1;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	type = sqlite3_column_int(*stmt, 0);
	if(type == TUP_NODE_GENERATED)
		rc = 0;
	else
		rc = 1;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_change_node(tupid_t tupid, const char *new_name, tupid_t new_dt)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CHANGE_NODE_NAME];
	static char s[] = "update node set name=?, dir=? where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m['%s', %lli, %lli][0m\n", s, new_name, new_dt, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, new_name, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, new_dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 3, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_get_type(tupid_t tupid, int *type)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_GET_TYPE];
	static char s[] = "select type from node where id=?";

	*type = -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	*type = sqlite3_column_int(*stmt, 0);
	rc = 0;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_set_type(tupid_t tupid, int type)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_TYPE];
	static char s[] = "update node set type=? where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %lli][0m\n", s, type, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_set_sym(tupid_t tupid, tupid_t sym)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_SYM];
	static char s[] = "update node set sym=? where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli][0m\n", s, sym, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, sym) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_set_mtime(tupid_t tupid, time_t mtime)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_MTIME];
	static char s[] = "update node set mtime=? where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%li, %lli][0m\n", s, mtime, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, mtime) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_print(FILE *stream, tupid_t tupid)
{
	int rc;

	fprintf(stream, " - [%lli] ", tupid);
	rc = db_print(stream, tupid);
	fprintf(stream, "\n");
	return rc;
}

int tup_db_alloc_generated_nodelist(char **s, int *len, tupid_t dt,
				    struct rb_root *tree)
{
	*s = NULL;
	*len = 0;
	*len = generated_nodelist_len(dt);
	if(*len < 0)
		return -1;
	if(*len == 0)
		return 0;
	/* The length may be an over-estimate, since it also contains any
	 * nodes scheduled to be deleted.
	 */
	*s = malloc(*len);
	if(!*s) {
		perror("malloc");
		return -1;
	}
	if(get_generated_nodelist(*s, dt, tree) < 0)
		return -1;
	return 0;
}

static int generated_nodelist_len(tupid_t dt)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODELIST_LEN];
	static char s[] = "select sum(length(name) + 2) from node where dir=? and type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, dt, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = sqlite3_column_int(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int get_generated_nodelist(char *dest, tupid_t dt, struct rb_root *tree)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_NODELIST];
	static char s[] = "select length(name), name, id from node where dir=? and type=?";
	char *p;
	int len;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, dt, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	p = dest;
	while(1) {
		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}
		if(tupid_tree_search(tree, sqlite3_column_int64(*stmt, 2)) != NULL) {
			continue;
		}
		*p = '/';
		p++;
		len = sqlite3_column_int(*stmt, 0);
		memcpy(p, sqlite3_column_text(*stmt, 1), len);
		p += len;
		*p = '\n';
		p++;
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_delete_gitignore(tupid_t dt, struct rb_root *tree)
{
	struct db_node dbn;
	struct tupid_tree *tt;

	if(tup_db_select_dbn(dt, ".gitignore", &dbn) < 0)
		return -1;
	/* Fine if the .gitignore file isn't present. */
	if(dbn.tupid < 0)
		return 0;

	tt = malloc(sizeof *tt);
	if(!tt) {
		perror("malloc");
		return -1;
	}
	tt->tupid = dbn.tupid;
	tupid_tree_insert(tree, tt);

	return 0;
}

static int db_print(FILE *stream, tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_PRINT];
	static char s[] = "select dir, type, name from node where id=?";
	tupid_t parent;
	int type;
	char *path;

	if(tupid == 0) {
		fprintf(stderr, "Error: Trying to tup_db_print(0)\n");
		return -1;
	}
	if(tupid == 1) {
		return 0;
	}
	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -ENOENT;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	parent = sqlite3_column_int64(*stmt, 0);
	type = sqlite3_column_int(*stmt, 1);
	path = strdup((const char *)sqlite3_column_text(*stmt, 2));
	if(!path) {
		perror("strdup");
		rc = -1;
		goto out_reset;
	}
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(db_print(stream, parent) < 0)
		return -1;

	if(parent != 1) {
		fprintf(stream, "/");
	}
	switch(type) {
		case TUP_NODE_DIR:
			fprintf(stream, "%s", path);
			break;
		case TUP_NODE_CMD:
			fprintf(stream, "[[34m%s[0m]", path);
			break;
		case TUP_NODE_GHOST:
			fprintf(stream, "[47;30m%s[0m", path);
			break;
		case TUP_NODE_FILE:
		case TUP_NODE_GENERATED:
		case TUP_NODE_VAR:
		default:
			fprintf(stream, "%s", path);
			break;
	}

	free(path);
	return 0;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_get_node_flags(tupid_t tupid)
{
	int rc;
	int flags = 0;

	rc = tup_db_in_create_list(tupid);
	if(rc < 0)
		return -1;
	if(rc == 1)
		flags |= TUP_FLAGS_CREATE;

	rc = tup_db_in_modify_list(tupid);
	if(rc < 0)
		return -1;
	if(rc == 1)
		flags |= TUP_FLAGS_MODIFY;

	return flags;
}

int tup_db_add_dir_create_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_DIR_CREATE_LIST];
	static char s[] = "insert or ignore into create_list select dir from node where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_add_create_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_CREATE_LIST];
	static char s[] = "insert or replace into create_list values(?)";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_add_modify_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_MODIFY_LIST];
	static char s[] = "insert or replace into modify_list values(?)";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_in_create_list(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_IN_CREATE_LIST];
	static char s[] = "select id from create_list where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = 1;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_in_modify_list(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_IN_MODIFY_LIST];
	static char s[] = "select id from modify_list where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = 1;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_unflag_create(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_CREATE];
	static char s[] = "delete from create_list where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_unflag_modify(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_MODIFY];
	static char s[] = "delete from modify_list where id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int get_recurse_dirs(tupid_t dt, struct list_head *list)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_RECURSE_DIRS];
	static char s[] = "select id from node where dir=? and type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, dt, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct id_entry *ide;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		ide = malloc(sizeof *ide);
		if(ide == NULL) {
			perror("malloc");
			return -1;
		}
		ide->tupid = sqlite3_column_int64(*stmt, 0);
		list_add(&ide->list, list);
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int get_dir_entries(tupid_t dt, struct list_head *list)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_DIR_ENTRIES];
	static char s[] = "select id from node where dir=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, dt);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct id_entry *ide;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		ide = malloc(sizeof *ide);
		if(ide == NULL) {
			perror("malloc");
			return -1;
		}
		ide->tupid = sqlite3_column_int64(*stmt, 0);
		list_add(&ide->list, list);
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_create_link(tupid_t a, tupid_t b, int style)
{
	int curstyle;

	if(tup_db_link_style(a, b, &curstyle) < 0)
		return -1;
	if(curstyle == -1)
		if(link_insert(a, b, style) < 0)
			return -1;
	if(! (curstyle & style)) {
		curstyle |= style;
		if(link_update(a, b, curstyle) < 0)
			return -1;
	}
	return 0;
}

int tup_db_create_unique_link(tupid_t a, tupid_t b, struct rb_root *tree)
{
	int rc;
	tupid_t incoming;

	rc = tup_db_get_incoming_link(b, &incoming);
	if(rc < 0)
		return -1;
	if(incoming != -1) {
		if(tupid_tree_search(tree, incoming) != NULL)
			incoming = -1;
	}
	/* See if we already own the link, or if the link doesn't exist yet */
	if(a == incoming || incoming == -1) {
		if(tup_db_add_write_list(b) < 0)
			return -1;
		return 0;
	}
	/* Otherwise, someone else got the girl. Err, output file. */
	fprintf(stderr, "Error: Unable to create a unique link from %lli to %lli because the destination is already linked to by node %lli.\n", a, b, incoming);
	return -1;
}

int tup_db_delete_empty_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_EMPTY_LINKS];
	static char s[] = "delete from link where to_id=? and style=0";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_yell_links(tupid_t tupid, struct rb_root *tree, const char *errmsg)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_YELL_LINKS];
	static char s[] = "select from_id, name from link, node where to_id=? and style=? and node.id=from_id and node.type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i, %i][0m\n", s, tupid, TUP_LINK_NORMAL, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_LINK_NORMAL) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = 0;
	while(1) {
		tupid_t badid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		badid = sqlite3_column_int64(*stmt, 0);
		if(tupid_tree_search(tree, badid) != NULL)
			continue;

		fprintf(stderr, "Error: %s\n", errmsg);
		fprintf(stderr, " -- File '%s' [%lli]\n",
			sqlite3_column_text(*stmt, 1), badid);
		fprintf(stderr, " -- Command ID: %lli\n", badid);
		rc = 1;
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_link_exists(tupid_t a, tupid_t b)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_LINK_EXISTS];
	static char s[] = "select to_id from link where from_id=? and to_id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli][0m\n", s, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc == SQLITE_DONE) {
		return -1;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_link_style(tupid_t a, tupid_t b, int *style)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_LINK_STYLE];
	static char s[] = "select style from link where from_id=? and to_id=?";

	*style = -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli][0m\n", s, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc == SQLITE_DONE) {
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	*style = sqlite3_column_int(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_get_incoming_link(tupid_t tupid, tupid_t *incoming)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_GET_INCOMING_LINK];
	static char s[] = "select from_id from link where to_id=?";

	*incoming = -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc == SQLITE_DONE) {
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	*incoming = sqlite3_column_int64(*stmt, 0);

	/* Do a quick double-check to make sure there isn't a duplicate link. */
	rc = sqlite3_step(*stmt);
	if(rc != SQLITE_DONE) {
		if(rc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}
		fprintf(stderr, "tup error: Node %lli is supposed to only have one incoming link, but multiple were found. The database is probably in a bad state. Sadness :(\n", tupid);
		return -1;
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_delete_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_LINKS];
	static char s[] = "delete from link where from_id=? or to_id=?";

	if(add_ghost_links(tupid) < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli][0m\n", s, tupid, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_unsticky_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNSTICKY_LINKS];
	static char s[] = "update link set style=style&~? where to_id=? and style&?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %lli, %i][0m\n", s, TUP_LINK_STICKY, tupid, TUP_LINK_STICKY);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int tree_add_tupids(struct rb_root *tree, sqlite3_stmt *stmt)
{
	int dbrc;

	while(1) {
		struct tupid_tree *tt;

		dbrc = sqlite3_step(stmt);
		if(dbrc == SQLITE_DONE) {
			return 0;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}

		tt = malloc(sizeof *tt);
		if(!tt) {
			perror("malloc");
			return -1;
		}
		tt->tupid = sqlite3_column_int64(stmt, 0);
		tupid_tree_insert(tree, tt);
	}
}

int tup_db_cmds_to_tree(tupid_t dt, struct rb_root *tree)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CMDS_TO_TREE];
	static char s[] = "select id from node where dir=? and type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, dt, TUP_NODE_CMD);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_CMD) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = tree_add_tupids(tree, *stmt);

	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_cmd_outputs_to_tree(tupid_t dt, struct rb_root *tree)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CMD_OUTPUTS_TO_TREE];
	static char s[] = "select to_id from link where from_id in (select id from node where dir=? and type=?)";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, dt, TUP_NODE_CMD);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_CMD) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = tree_add_tupids(tree, *stmt);

	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_modify_cmds_by_output(tupid_t output, int *modified)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MODIFY_CMDS_BY_OUTPUT];
	static char s[] = "insert or ignore into modify_list select from_id from link where to_id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, output);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, output) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(modified)
		*modified = sqlite3_changes(tup_db);

	return 0;
}

int tup_db_modify_cmds_by_input(tupid_t input)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MODIFY_CMDS_BY_INPUT];
	static char s[] = "insert or replace into modify_list select to_id from link, node where from_id=? and to_id=id and type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, input, TUP_NODE_CMD);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, input) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_CMD) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_set_dependent_dir_flags(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_DEPENDENT_DIR_FLAGS];
	static char s[] = "insert or replace into create_list select to_id from link, node where from_id=? and to_id=id and type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, tupid, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_select_node_by_link(int (*callback)(void *, struct db_node *,
					       int style),
			       void *arg, tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_BY_LINK];
	static char s[] = "select id, dir, name, type, sym, mtime, style from node, link where from_id=? and id=to_id";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		struct db_node dbn;
		int style;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		dbn.tupid = sqlite3_column_int64(*stmt, 0);
		dbn.dt = sqlite3_column_int64(*stmt, 1);
		dbn.name = (const char *)sqlite3_column_text(*stmt, 2);
		dbn.type = sqlite3_column_int(*stmt, 3);
		dbn.sym = sqlite3_column_int64(*stmt, 4);
		dbn.mtime = sqlite3_column_int64(*stmt, 5);
		style = sqlite3_column_int(*stmt, 6);

		if(callback(arg, &dbn, style) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_delete_dependent_dir_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_DEPENDENT_DIR_LINKS];
	static char s[] = "delete from link where to_id=?";

	if(add_ghost_links(tupid) < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int config_cb(void *arg, int argc, char **argv, char **col)
{
	int x;
	char *lval = NULL;
	char *rval = NULL;
	if(arg) {/* unused */}

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "lval") == 0)
			lval = argv[x];
		if(strcmp(col[x], "rval") == 0)
			rval = argv[x];
	}
	printf("%s: '%s'\n", lval, rval);
	return 0;
}

int tup_db_show_config(void)
{
	char *errmsg;
	char s[] = "select * from config";

	if(sqlite3_exec(tup_db, s, config_cb, NULL, &errmsg) != 0) {
		fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
			errmsg, s);
		sqlite3_free(errmsg);
		return -1;
	}
	return 0;
}

int tup_db_config_set_int(const char *lval, int x)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_SET_INT];
	static char s[] = "insert or replace into config values(?, ?)";

	if(sql_debug) fprintf(stderr, "%s [37m['%s', %i][0m\n", s, lval, x);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, x) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_config_get_int(const char *lval)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_GET_INT];
	static char s[] = "select rval from config where lval=?";

	if(sql_debug) fprintf(stderr, "%s [37m['%s'][0m\n", s, lval);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -1;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = sqlite3_column_int(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_config_set_int64(const char *lval, sqlite3_int64 x)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_SET_INT64];
	static char s[] = "insert or replace into config values(?, ?)";

	if(sql_debug) fprintf(stderr, "%s [37m['%s', %lli][0m\n", s, lval, x);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, x) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

sqlite3_int64 tup_db_config_get_int64(const char *lval)
{
	sqlite3_int64 rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_GET_INT64];
	static char s[] = "select rval from config where lval=?";

	if(sql_debug) fprintf(stderr, "%s [37m['%s'][0m\n", s, lval);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -1;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = sqlite3_column_int64(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_config_set_string(const char *lval, const char *rval)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_SET_STRING];
	static char s[] = "insert or replace into config values(?, ?)";

	if(sql_debug) fprintf(stderr, "%s [37m['%s', '%s'][0m\n", s, lval, rval);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, rval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_config_get_string(char **res, const char *lval, const char *def)
{
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_GET_STRING];
	static char s[] = "select rval from config where lval=?";

	if(sql_debug) fprintf(stderr, "%s [37m['%s'][0m\n", s, lval);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		*res = strdup(def);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		*res = NULL;
		goto out_reset;
	}

	*res = strdup((const char *)sqlite3_column_text(*stmt, 0));

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(*res)
		return 0;
	return -1;
}

int tup_db_set_var(tupid_t tupid, const char *value)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_VAR];
	static char s[] = "insert or replace into var values(?, ?)";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, '%s'][0m\n", s, tupid, value);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, value, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	tup_db_var_changed++;

	return 0;
}

static int get_var_id(tupid_t tupid, char **dest)
{
	int rc = -1;
	int dbrc;
	int len;
	const char *value;
	sqlite3_stmt **stmt = &stmts[_DB_GET_VAR_ID];
	static char s[] = "select value, length(value) from var where var.id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable id %lli not found in .tup/db.\n", tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	len = sqlite3_column_int(*stmt, 1);
	if(len < 0) {
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(*stmt, 0);
	if(!value) {
		goto out_reset;
	}
	memcpy(*dest, value, len);
	*dest += len;
	rc = 0;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

tupid_t tup_db_get_var(const char *var, int varlen, char **dest)
{
	struct db_node dbn;

	if(node_select(VAR_DT, var, varlen, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0) {
		return tup_db_node_insert(VAR_DT, var, varlen, TUP_NODE_GHOST, -1);
	}
	if(dbn.type == TUP_NODE_GHOST)
		return dbn.tupid;
	if(get_var_id(dbn.tupid, dest) < 0)
		return -1;
	return dbn.tupid;
}

int tup_db_get_var_id_alloc(tupid_t tupid, char **dest)
{
	int rc = -1;
	int dbrc;
	int len;
	const char *value;
	sqlite3_stmt **stmt = &stmts[DB_GET_VAR_ID_ALLOC];
	static char s[] = "select value, length(value) from var where var.id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable id %lli not found in .tup/db.\n", tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	len = sqlite3_column_int(*stmt, 1);
	if(len < 0) {
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(*stmt, 0);
	if(!value) {
		goto out_reset;
	}
	*dest = malloc(len + 1);
	if(!*dest) {
		perror("malloc");
		goto out_reset;
	}
	memcpy(*dest, value, len);
	(*dest)[len] = 0;
	rc = 0;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

int tup_db_get_varlen(const char *var, int varlen)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_GET_VARLEN];
	static char s[] = "select length(value) from var, node where node.dir=? and node.name=? and node.id=var.id";

	if(sql_debug) fprintf(stderr, "%s [37m[%i, '%.*s'][0m\n", s, VAR_DT, varlen, var);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		/* Non-existent variable has zero length. This will be made
		 * into a ghost node.
		 */
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	rc = sqlite3_column_int(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

tupid_t tup_db_write_var(const char *var, int varlen, int fd)
{
	int dbrc;
	int len;
	const char *value;
	sqlite3_stmt **stmt = &stmts[DB_WRITE_VAR];
	static char s[] = "select var.id, value, length(value) from var, node where node.dir=? and node.name=? and node.id=var.id";
	tupid_t tupid = -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, '%.*s'][0m\n", s, tupid, varlen, var);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, var, varlen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"Error: Variable '%.*s' not found in .tup/db.\n",
			varlen, var);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		goto out_reset;
	}

	len = sqlite3_column_int(*stmt, 2);
	if(len < 0) {
		goto out_reset;
	}
	value = (const char *)sqlite3_column_text(*stmt, 1);
	if(!value) {
		goto out_reset;
	}

	/* Hack for binary values in varsed rules? Used for binutils */
	if(len == 1) {
		if(value[0] == 'y')
			value = "1";
		else if(value[0] == 'n')
			value = "0";
	}

	if(write(fd, value, len) == len)
		tupid = sqlite3_column_int64(*stmt, 0);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return tupid;
}

int tup_db_var_foreach(int (*callback)(void *, const char *var, const char *value), void *arg)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_VAR_FOREACH];
	static char s[] = "select name, value from var, node where node.dir=? and node.id=var.id order by name";

	if(sql_debug) fprintf(stderr, "%s [37m[%i][0m\n", s, VAR_DT);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(1) {
		const char *var;
		const char *value;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		var = (const char *)sqlite3_column_text(*stmt, 0);
		value = (const char *)sqlite3_column_text(*stmt, 1);

		if((rc = callback(arg, var, value)) < 0) {
			goto out_reset;
		}
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int write_vars_cb(void *arg, const char *var, const char *value)
{
	struct var_list *varlist = arg;
	struct var_entry *entry;

	varlist->count++;
	entry = malloc(sizeof *entry);
	if(!entry) {
		perror("malloc");
		return -1;
	}
	entry->var = strdup(var);
	if(!entry->var) {
		perror("strdup");
		return -1;
	}
	entry->value = strdup(value);
	if(!entry->value) {
		perror("strdup");
		return -1;
	}
	entry->varlen = strlen(entry->var);
	entry->vallen = strlen(entry->value);
	list_add_tail(&entry->list, &varlist->vars);
	return 0;
}

int tup_db_write_vars(void)
{
	int fd;
	int rc = -1;
	struct var_list varlist;
	struct var_entry *ent;
	unsigned int x;

	if(tup_db_var_changed == 0)
		return 0;

	INIT_LIST_HEAD(&varlist.vars);
	varlist.count = 0;

	/* Already at the top of the tup hierarchy because of find_tup_dir() */
	fd = creat(TUP_VARDICT_FILE, 0666);
	if(fd < 0) {
		perror("creat");
		return -1;
	}
	if(tup_db_var_foreach(write_vars_cb, &varlist) < 0)
		goto out_err;
	if(write(fd, &varlist.count, sizeof(varlist.count)) != sizeof(varlist.count)) {
		perror("write");
		goto out_err;
	}
	/* Write out index */
	x = 0;
	list_for_each_entry(ent, &varlist.vars, list) {
		if(write(fd, &x, sizeof(x)) != sizeof(x)) {
			perror("write");
			goto out_err;
		}
		/* each line is 'variable=value', so +1 for the equals sign,
		 * and +1 for the newline.
		 */
		x += ent->varlen + 1 + ent->vallen + 1;
	}

	/* Write out the variables */
	list_for_each_entry(ent, &varlist.vars, list) {
		if(write(fd, ent->var, ent->varlen) != ent->varlen) {
			perror("write");
			goto out_err;
		}
		if(write(fd, "=", 1) != 1) {
			perror("write");
			goto out_err;
		}
		if(write(fd, ent->value, ent->vallen) != ent->vallen) {
			perror("write");
			goto out_err;
		}
		if(write(fd, "\0", 1) != 1) {
			perror("write");
			goto out_err;
		}
	}

	rc = 0;
out_err:
	close(fd);
	return rc;
}

int tup_db_var_pre(void)
{
	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_request_tmp_list() < 0)
		return -1;
	if(init_var_list() < 0)
		return -1;
	return 0;
}

int tup_db_var_post(void)
{
	/* This is similar to tup_del_id(), but specific to TUP_NODE_VAR. */
	if(var_list_flag_dirs() < 0)
		return -1;
	if(var_list_flag_cmds() < 0)
		return -1;
	if(var_list_unflag_create() < 0)
		return -1;
	if(var_list_unflag_modify() < 0)
		return -1;
	if(var_list_delete_links() < 0)
		return -1;
	if(var_list_delete_vars() < 0)
		return -1;
	if(var_list_delete_nodes() < 0)
		return -1;
	if(tup_db_clear_tmp_list() < 0)
		return -1;
	if(tup_db_commit() < 0)
		return -1;
	if(tup_db_release_tmp_list() < 0)
		return -1;
	return 0;
}

int tup_db_scan_begin(void)
{
	if(tup_db_request_tmp_list() < 0)
		return -1;
	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_files_to_tmp() < 0)
		return -1;
	if(tup_db_unflag_tmp(DOT_DT) < 0)
		return -1;
	if(tup_db_unflag_tmp(VAR_DT) < 0)
		return -1;
	return 0;
}

int tup_db_scan_end(void)
{
	struct half_entry *he;
	LIST_HEAD(del_list);

	if(tup_db_get_all_in_tmp(&del_list) < 0)
		return -1;
	while(!list_empty(&del_list)) {
		he = list_entry(del_list.next, struct half_entry, list);
		if(tup_del_id(he->tupid) < 0)
			return -1;
		list_del(&he->list);
		free(he);
	}

	if(tup_db_commit() < 0)
		return -1;
	if(tup_db_release_tmp_list() < 0)
		return -1;
	return 0;
}

static int tmp_list_out = 0;
int tup_db_request_tmp_list(void)
{
	if(tmp_list_out) {
		fprintf(stderr, "[31mtup internal error: tmp list already in use.[0m\n");
		return -1;
	}
	tmp_list_out = 1;
	return 0;
}

int tup_db_release_tmp_list(void)
{
	if(!tmp_list_out) {
		fprintf(stderr, "[31mtup internal error: tmp list not in use[0m\n");
		return -1;
	}
	tmp_list_out = 0;
	return 0;
}

static int check_tmp_requested(void)
{
	if(!tmp_list_out) {
		fprintf(stderr, "[31mtup internal error: tmp list hasn't been requested before use[0m\n");
		return -1;
	}
	return 0;
}

int tup_db_files_to_tmp(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_FILES_TO_TMP];
	static char s[] = "insert into tmp_list select id from node where type=? or type=? or type=? and name <> '.gitignore'";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %i, %i][0m\n", s, TUP_NODE_FILE, TUP_NODE_DIR, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_FILE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_unflag_tmp(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_TMP];
	static char s[] = "delete from tmp_list where tmpid=?";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_get_all_in_tmp(struct list_head *list)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_GET_ALL_IN_TMP];
	static char s[] = "select node.id, dir, sym, type from tmp_list, node where tmp_list.tmpid = node.id";
	struct half_entry *he;

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	while(1) {
		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			rc = -1;
			goto out_reset;
		}

		he = malloc(sizeof *he);
		if(he == NULL) {
			perror("malloc");
			return -1;
		}
		he->tupid = sqlite3_column_int64(*stmt, 0);
		he->dt = sqlite3_column_int64(*stmt, 1);
		he->sym = sqlite3_column_int64(*stmt, 2);
		he->type = sqlite3_column_int(*stmt, 3);
		list_add(&he->list, list);
	}

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	return rc;
}

int tup_db_clear_tmp_list(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CLEAR_TMP_LIST];
	static char s[] = "delete from tmp_list";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_add_write_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_WRITE_LIST];
	static char s[] = "insert or replace into tmp_list values(?)";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

int tup_db_check_write_list(tupid_t cmdid)
{
	int rc = 0;
	if(check_expected_outputs(cmdid) < 0)
		rc = -1;
	if(check_actual_outputs(cmdid) < 0)
		rc = -1;
	return rc;
}

int tup_db_add_read_list(tupid_t tupid)
{
	return tup_db_add_write_list(tupid);
}

int tup_db_check_read_list(tupid_t cmdid)
{
	if(check_actual_inputs(cmdid) < 0)
		return -1;
	if(add_input_links(cmdid) < 0)
		return -1;
	if(style_input_links(cmdid) < 0)
		return -1;
	if(drop_old_links(cmdid) < 0)
		return -1;
	if(unstyle_old_links(cmdid) < 0)
		return -1;
	return 0;
}

int tup_db_write_outputs(tupid_t cmdid)
{
	int outputs_differ = 0;

	if(add_outputs(cmdid, &outputs_differ) < 0)
		return -1;
	if(remove_outputs(cmdid, &outputs_differ) < 0)
		return -1;
	if(outputs_differ == 1) {
		if(tup_db_add_modify_list(cmdid) < 0)
			return -1;
	}
	return 0;
}

tupid_t tup_db_node_insert(tupid_t dt, const char *name, int len, int type,
			   time_t mtime)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_NODE_INSERT];
	static char s[] = "insert into node(dir, type, name, sym, mtime) values(?, ?, ?, -1, ?)";
	tupid_t tupid;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i, '%.*s', %li][0m\n", s, dt, type, len, name, mtime);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 3, name, len, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 4, mtime) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	tupid = sqlite3_last_insert_rowid(tup_db);
	switch(type) {
		/* New commands go in the modify list so they are executed at
		 * least once.
		 */
		case TUP_NODE_CMD:
			if(tup_db_add_modify_list(tupid) < 0)
				return -1;
			break;
	}

	return tupid;
}

static int node_select(tupid_t dt, const char *name, int len,
		       struct db_node *dbn)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODE_SELECT];
	static char s[] = "select id, type, sym, mtime from node where dir=? and name=?";

	dbn->tupid = -1;
	dbn->dt = -1;
	dbn->name = NULL;
	dbn->type = 0;
	dbn->sym = -1;
	dbn->mtime = -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, '%.*s'][0m\n", s, dt, len, name);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, name, len, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}

	rc = 0;
	dbn->tupid = sqlite3_column_int64(*stmt, 0);
	dbn->type = sqlite3_column_int64(*stmt, 1);
	dbn->sym = sqlite3_column_int64(*stmt, 2);
	dbn->mtime = sqlite3_column_int64(*stmt, 3);

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int link_insert(tupid_t a, tupid_t b, int style)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_LINK_INSERT];
	static char s[] = "insert into link(from_id, to_id, style) values(?, ?, ?)";

	if(a == b) {
		fprintf(stderr, "tup error: Attempt made to link a node to itself (%lli)\n", a);
		return -1;
	}
	if(style == 0) {
		fprintf(stderr, "tup error: Attempt to insert unstyled link %lli -> %lli\n", a, b);
		return -1;
	}

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli, %i][0m\n", s, a, b, style);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, style) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int link_update(tupid_t a, tupid_t b, int style)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_LINK_UPDATE];
	static char s[] = "update link set style=? where from_id=? and to_id=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %lli, %lli][0m\n", s, style, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, style) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 3, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int node_has_ghosts(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODE_HAS_GHOSTS];
	static char s[] = "select id from node where dir=? or sym=?";

	/* This is used to determine if we need to make a real node into a
	 * ghost node. We only need to do that if some other node references it
	 * via dir or sym. We don't care about links because nothing will have
	 * a link to a ghost.
	 */
	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli][0m\n", s, tupid, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		rc = -1;
		goto out_reset;
	}
	rc = 1;

out_reset:
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int create_ghost_list(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_CREATE_GHOST_LIST];
	static char s[] = "create temporary table ghost_list (id integer primary key not null)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int create_tmp_list(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_CREATE_TMP_LIST];
	static char s[] = "create temporary table tmp_list (tmpid integer primary key not null)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int add_ghost_dt_sym(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_ADD_GHOST_DT_SYM];
	static char s[] = "insert or ignore into ghost_list select id from node where id in (select dir from node where id=? union select sym from node where id=?) and type=?";

	if(tupid < 0)
		return 0;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %lli, %i][0m\n", s, tupid, tupid, TUP_NODE_GHOST);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GHOST) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	num_ghosts += sqlite3_changes(tup_db);

	return 0;
}

static int add_ghost_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_ADD_GHOST_LINKS];
	static char s[] = "insert or replace into ghost_list select id from node where id in (select from_id from link where to_id=?) and type=?";

	if(tupid < 0)
		return 0;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, tupid, TUP_NODE_GHOST);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_GHOST) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	num_ghosts += sqlite3_changes(tup_db);

	return 0;
}

static int add_ghost_dirs(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_ADD_GHOST_DIRS];
	static char s[] = "insert or replace into ghost_list select id from node where id in (select dir from ghost_list left join node on ghost_list.id=node.id) and type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i][0m\n", s, TUP_NODE_GHOST);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_GHOST) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	num_ghosts += sqlite3_changes(tup_db);

	return 0;
}

static int reclaim_ghosts(void)
{
	int rc;
	int changes;
	sqlite3_stmt **stmt = &stmts[_DB_RECLAIM_GHOSTS];
	static char s[] = "delete from node where id in (select gid from (select ghost_list.id as gid from ghost_list left join node on dir=ghost_list.id left join link on from_id=ghost_list.id where dir is null and from_id is null) left join node on sym=gid where sym is null)";
	/* All the nodes in ghost_list already are of type TUP_NODE_GHOST. Just
	 * make sure they are no longer needed before deleting them by checking:
	 *  - no other node references it in 'dir'
	 *  - no other node references it in 'sym'
	 *  - no other node is pointed to by us
	 *
	 * If all those cases check out then the ghost can be removed. This is
	 * done in a loop until no ghosts are removed in order to handle things
	 * like a ghost dir having a ghost subdir - the subdir would be removed
	 * in one pass, then the other dir in the next pass.
	 */

	if(!num_ghosts)
		return 0;

	/* Make sure any ghost that is in a ghost directory also has its
	 * parent directory checked (t2040).
	 */
	if(add_ghost_dirs() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	do {
		rc = sqlite3_step(*stmt);
		if(sqlite3_reset(*stmt) != 0) {
			fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}
		if(rc != SQLITE_DONE) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}
		changes = sqlite3_changes(tup_db);
	} while(changes > 0);

	return clear_ghost_list();
}

static int clear_ghost_list(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_CLEAR_GHOST_LIST];
	static char s[] = "delete from ghost_list";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	num_ghosts = 0;

	return 0;
}

static int init_var_list(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_INIT_VAR_LIST];
	static char s[] = "insert or replace into tmp_list select id from node where dir=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i][0m\n", s, VAR_DT);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, VAR_DT) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_flag_dirs(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_FLAG_DIRS];
	static char s[] = "insert or replace into create_list select to_id from link, node where from_id in (select tmpid from tmp_list) and to_id=node.id and node.type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i][0m\n", s, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_flag_cmds(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_FLAG_CMDS];
	static char s[] = "insert or replace into modify_list select to_id from link, node where from_id in (select tmpid from tmp_list) and to_id=node.id and node.type=?";

	if(sql_debug) fprintf(stderr, "%s [37m[%i][0m\n", s, TUP_NODE_CMD);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_CMD) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_unflag_create(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_UNFLAG_CREATE];
	static char s[] = "delete from create_list where id in (select tmpid from tmp_list)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_unflag_modify(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_UNFLAG_MODIFY];
	static char s[] = "delete from modify_list where id in (select tmpid from tmp_list)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_delete_links(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_DELETE_LINKS];
	static char s[] = "delete from link where from_id in (select tmpid from tmp_list)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_delete_vars(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_DELETE_VARS];
	static char s[] = "delete from var where id in (select tmpid from tmp_list)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int var_list_delete_nodes(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_LIST_DELETE_NODES];
	static char s[] = "delete from node where id in (select tmpid from tmp_list)";

	if(sql_debug) fprintf(stderr, "%s\n", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	tup_db_var_changed += sqlite3_changes(tup_db);

	return 0;
}

static int check_expected_outputs(tupid_t cmdid)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_CHECK_EXPECTED_OUTPUTS];
	static char s[] = "select to_id, name from link, node left join tmp_list on tmpid=to_id where from_id=? and id=to_id and tmpid is null";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, cmdid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	do {
		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}

		fprintf(stderr, "Bork: Expected to write to file '%s' from cmd %lli but didn't\n", sqlite3_column_text(*stmt, 1), cmdid);
		rc = -1;
	} while(1);

	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

struct del_entry {
	struct list_head list;
	tupid_t tupid;
	tupid_t dt;
	const char *name;
};
static int check_actual_outputs(tupid_t cmdid)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_CHECK_ACTUAL_OUTPUTS];
	static char s[] = "select tmpid, dir, name from tmp_list, node where tmpid not in (select to_id from link where from_id=?) and tmpid=id";
	struct del_entry *de;
	LIST_HEAD(del_list);

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, cmdid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	do {
		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}

		de = malloc(sizeof *de);
		if(!de) {
			perror("malloc");
			fprintf(stderr, "Unable to properly remove file '%s' from the filesystem.\n", sqlite3_column_text(*stmt, 2));
			return -1;
		}
		de->tupid = sqlite3_column_int64(*stmt, 0);
		de->dt = sqlite3_column_int64(*stmt, 1);
		de->name = strdup((const char *)sqlite3_column_text(*stmt, 2));

		if(!de->name) {
			perror("strdup");
			fprintf(stderr, "Unable to properly remove file '%s' from the filesystem.\n", sqlite3_column_text(*stmt, 2));
		}
		list_add(&de->list, &del_list);

		if(rc != -1) {
			/* rc test used to only print this once */
			fprintf(stderr, "tup error: Unspecified output files - A command is writing to files that you    didn't specify in the Tupfile. You should add them so tup knows what to expect.\n");
			fprintf(stderr, " -- Command ID: %lli\n", cmdid);
		}

		fprintf(stderr, " -- File: '%s' [%lli in dir %lli]\n", de->name, de->tupid, de->dt);
		rc = -1;
	} while(1);

	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	while(!list_empty(&del_list)) {
		/* TODO: replace tup_db_modify_cmds_by_output with a single
		 * sql call to modify all cmds generated incorrect nodes?
		 */
		de = list_entry(del_list.next, struct del_entry, list);

		/* Clear the sym field in case we wrote a bad symlink (t5032) */
		tup_db_set_sym(de->tupid, -1);

		/* Re-run whatever command was supposed to create this file (if
		 * any), and remove the bad output. This is particularly
		 * helpful if a symlink was created in the wrong spot.
		 */
		tup_db_modify_cmds_by_output(de->tupid, NULL);
		fprintf(stderr, "[35m -- Delete: %s at dir %lli[0m\n",
			de->name, de->dt);
		delete_file(de->dt, de->name);
		list_del(&de->list);
		free(de);
	}

	return rc;
}

static int check_actual_inputs(tupid_t cmdid)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_CHECK_ACTUAL_INPUTS];
	static char s[] = "select tmpid, dir, name from tmp_list, node where tmpid not in (select id from node where id=tmpid and type!=?) and tmpid not in (select from_id from link where from_id=tmpid and to_id=? and style&?) and tmpid=id";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %lli, %i][0m\n", s, TUP_NODE_GENERATED, cmdid, TUP_LINK_STICKY);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	do {
		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}

		if(rc != -1) {
			/* rc test used to only print this once */
			fprintf(stderr, "tup error: Missing input dependency - a file was read from, and was not         specified as an input link for the command. This is an issue because the file   was created from another command, and without the input link the commands may   execute out of order. You should add this file as an input, since it is         possible this could randomly break in the future.\n");
			fprintf(stderr, " - Command ID: %lli\n", cmdid);
		}

		tup_db_print(stderr, sqlite3_column_int64(*stmt, 0));
		rc = -1;
	} while(1);

	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return rc;
}

static int add_input_links(tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_ADD_INPUT_LINKS];
	static char s[] = "insert into link select tmpid, ?, ? from tmp_list where tmpid not in (select from_id from link where to_id=?)";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i, %lli][0m\n", s, cmdid, TUP_LINK_NORMAL, cmdid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_LINK_NORMAL) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 3, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int style_input_links(tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_STYLE_INPUT_LINKS];
	static char s[] = "update link set style=? where from_id in (select tmpid from tmp_list) and to_id=? and style=?";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %lli, %i][0m\n", s, TUP_LINK_NORMAL|TUP_LINK_STICKY, cmdid, TUP_LINK_STICKY);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_LINK_NORMAL|TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int drop_old_links(tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_DROP_OLD_LINKS];
	static char s[] = "delete from link where from_id not in (select tmpid from tmp_list) and to_id=? and style=?";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, cmdid, TUP_LINK_NORMAL);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_LINK_NORMAL) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int unstyle_old_links(tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_UNSTYLE_OLD_LINKS];
	static char s[] = "update link set style=? where from_id not in (select tmpid from tmp_list) and to_id=? and style=?";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%i, %lli, %i][0m\n", s, TUP_LINK_STICKY, cmdid, TUP_LINK_NORMAL|TUP_LINK_STICKY);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_LINK_NORMAL|TUP_LINK_STICKY) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	return 0;
}

static int add_outputs(tupid_t cmdid, int *outputs_differ)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_ADD_OUTPUTS];
	static char s[] = "insert or ignore into link select ?, tmpid, ? from tmp_list";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli, %i][0m\n", s, cmdid, TUP_LINK_NORMAL);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, TUP_LINK_NORMAL) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(sqlite3_changes(tup_db))
		*outputs_differ = 1;

	return 0;
}

static int remove_outputs(tupid_t cmdid, int *outputs_differ)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_REMOVE_OUTPUTS];
	static char s[] = "delete from link where from_id=? and to_id not in (select tmpid from tmp_list)";

	if(check_tmp_requested() < 0)
		return -1;

	if(sql_debug) fprintf(stderr, "%s [37m[%lli][0m\n", s, cmdid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(tup_db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		return -1;
	}

	if(sqlite3_changes(tup_db))
		*outputs_differ = 1;

	return 0;
}

static int no_sync(void)
{
	char *errmsg;
	char sql[] = "PRAGMA synchronous=OFF";

	if(sql_debug) fprintf(stderr, "%s\n", sql);
	if(sqlite3_exec(tup_db, sql, NULL, NULL, &errmsg) != 0) {
		fprintf(stderr, "SQL error: %s\nQuery was: %s",
			errmsg, sql);
		return -1;
	}
	return 0;
}
