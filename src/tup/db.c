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
#include "db.h"
#include "array_size.h"
#include "tupid_tree.h"
#include "fileio.h"
#include "config.h"
#include "vardb.h"
#include "fslurp.h"
#include "entry.h"
#include "graph.h"
#include "version.h"
#include "platform.h"
#include "monitor.h"
#include "container.h"
#include "option.h"
#include "environ.h"
#include "timespan.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "sqlite3/sqlite3.h"

#define DB_VERSION 14
#define PARSER_VERSION 4

enum {
	DB_BEGIN,
	DB_COMMIT,
	DB_ROLLBACK,
	DB_FILL_TUP_ENTRY,
	DB_SELECT_NODE_BY_FLAGS_1,
	DB_SELECT_NODE_BY_FLAGS_2,
	DB_SELECT_NODE_BY_FLAGS_3,
	DB_SELECT_NODE_DIR,
	DB_SELECT_NODE_DIR_GLOB,
	DB_DELETE_NODE,
	DB_MODIFY_DIR,
	DB_OPEN_TUPID,
	DB_CHANGE_NODE_NAME,
	DB_SET_NAME,
	DB_SET_TYPE,
	DB_SET_MTIME,
	DB_PRINT,
	DB_REBUILD_ALL,
	_DB_NODELIST_LEN,
	_DB_GET_NODELIST,
	DB_ADD_DIR_CREATE_LIST,
	DB_ADD_CONFIG_LIST,
	DB_ADD_CREATE_LIST,
	DB_ADD_MODIFY_LIST,
	DB_ADD_VARIANT_LIST,
	DB_IN_CONFIG_LIST,
	DB_IN_CREATE_LIST,
	DB_IN_MODIFY_LIST,
	DB_UNFLAG_CONFIG,
	DB_UNFLAG_CREATE,
	DB_UNFLAG_MODIFY,
	DB_UNFLAG_VARIANT,
	_DB_GET_RECURSE_DIRS,
	_DB_GET_DIR_ENTRIES,
	DB_LINK_EXISTS,
	DB_LINK_STYLE,
	DB_GET_INCOMING_LINK,
	DB_DELETE_LINKS,
	DB_DIRTYPE_TO_TREE,
	DB_MODIFY_CMDS_BY_OUTPUT,
	DB_MODIFY_CMDS_BY_INPUT,
	DB_SET_DEPENDENT_DIR_FLAGS,
	DB_SELECT_NODE_BY_LINK,
	DB_CONFIG_SET_INT,
	DB_CONFIG_GET_INT,
	DB_CONFIG_SET_STRING,
	DB_SET_VAR,
	_DB_GET_VAR_ID,
	DB_GET_VAR_ID_ALLOC,
	DB_WRITE_VAR,
	DB_VAR_FOREACH,
	DB_FILES_TO_TREE,
	_DB_GET_OUTPUT_TREE,
	_DB_GET_LINKS,
	DB_NODE_INSERT,
	_DB_NODE_SELECT,
	_DB_LINK_INSERT,
	_DB_LINK_UPDATE,
	_DB_LINK_REMOVE,
	_DB_NODE_HAS_GHOSTS,
	_DB_ADD_GHOST_LINKS,
	_DB_GHOST_RECLAIMABLE1,
	_DB_GHOST_RECLAIMABLE2,
	_DB_GET_DB_VAR_TREE,
	_DB_VAR_FLAG_DIRS,
	_DB_DELETE_VAR_ENTRY,
	DB_NUM_STATEMENTS
};

struct id_entry {
	LIST_ENTRY(id_entry) list;
	tupid_t tupid;
};
LIST_HEAD(id_entry_head, id_entry);

struct half_entry {
	LIST_ENTRY(half_entry) list;
	tupid_t tupid;
	tupid_t dt;
	int type;
};
LIST_HEAD(half_entry_head, half_entry);

static sqlite3 *tup_db = NULL;
static sqlite3_stmt *stmts[DB_NUM_STATEMENTS];
static struct tup_entry_head ghost_list;
static int tup_db_var_changed = 0;
static int sql_debug = 0;
static int reclaim_ghost_debug = 0;
static struct vardb atvardb = { {NULL}, 0};
static struct vardb envdb = { {NULL}, 0};
static int transaction = 0;
static tupid_t local_slash_dt = -1;

static int version_check(void);
static int node_select(tupid_t dt, const char *name, int len,
		       struct tup_entry **entry);

static int link_insert(tupid_t a, tupid_t b, int style);
static int link_update(tupid_t a, tupid_t b, int style);
static int link_remove(tupid_t a, tupid_t b);
static int node_has_ghosts(tupid_t tupid);
static int files_to_tree(struct tupid_entries *root);
static int add_ghost(tupid_t tupid);
static int add_ghost_links(tupid_t tupid);
static int reclaim_ghosts(void);
static int ghost_reclaimable(tupid_t tupid);
static int ghost_reclaimable1(tupid_t tupid);
static int ghost_reclaimable2(tupid_t tupid);
static int get_db_var_tree(tupid_t dt, struct vardb *vdb);
static int get_file_var_tree(struct vardb *vdb, int fd);
static int var_flag_dirs(tupid_t tupid);
static int delete_var_entry(tupid_t tupid);
static int no_sync(void);
static int delete_node(tupid_t tupid);
static int generated_nodelist_len(tupid_t dt);
static int get_generated_nodelist(char *dest, tupid_t dt,
				  struct tupid_entries *root, int *total_len);
static int db_print(FILE *stream, tupid_t tupid);
static int get_recurse_dirs(tupid_t dt, struct id_entry_head *head);
static int get_dir_entries(tupid_t dt, struct half_entry_head *head);

static char transaction_buf[1024];
static struct timespan transaction_ts;

static void transaction_check(const char *format, ...)
{
	if(sql_debug || !transaction) {
		va_list ap;
		va_start(ap, format);

		timespan_start(&transaction_ts);
		vsnprintf(transaction_buf, sizeof(transaction_buf), format, ap);
		transaction_buf[sizeof(transaction_buf)-1] = 0;
		va_end(ap);
		if(!transaction) {
			fprintf(stderr, "[41m%s[0m\n", transaction_buf);
			fprintf(stderr, "tup internal error: Database query must be in a transaction.\n");
			exit(1);
		}
	}
}

static int msqlite3_reset(sqlite3_stmt *stmt)
{
	if(sql_debug) {
		timespan_end(&transaction_ts);
		fprintf(stderr, "[%fs] %s\n", timespan_seconds(&transaction_ts), transaction_buf);
	}
	return sqlite3_reset(stmt);
}

int tup_db_open(void)
{
	int rc;
	int x;
	int db_sync;

	rc = sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != 0) {
		fprintf(stderr, "Unable to open database: %s\n",
			sqlite3_errmsg(tup_db));
	}
	for(x=0; x<ARRAY_SIZE(stmts); x++) {
		stmts[x] = NULL;
	}

	db_sync = tup_option_get_flag("db.sync");
	if(db_sync == 0)
		if(no_sync() < 0)
			return -1;
	if(tup_db_begin() < 0)
		return -1;
	if(version_check() < 0)
		return -1;
	if(tup_db_commit() < 0)
		return -1;

	return rc;
}

int tup_db_close(void)
{
	int x;

	for(x=0; x<ARRAY_SIZE(stmts); x++) {
		if(stmts[x])
			sqlite3_finalize(stmts[x]);
	}

	if(sqlite3_close(tup_db) != 0) {
		fprintf(stderr, "Unable to close database: %s\n",
			sqlite3_errmsg(tup_db));
		return -1;
	}
	return 0;
}

int tup_db_create(int db_sync)
{
	int rc;
	int x;
	const char *sql[] = {
		"create table node (id integer primary key not null, dir integer not null, type integer not null, mtime integer not null, srcid integer not null, name varchar(4096), unique(dir, name))",
		"create table link (from_id integer, to_id integer, style integer, unique(from_id, to_id))",
		"create table var (id integer primary key not null, value varchar(4096))",
		"create table config(lval varchar(256) unique, rval varchar(256))",
		"create table config_list (id integer primary key not null)",
		"create table create_list (id integer primary key not null)",
		"create table modify_list (id integer primary key not null)",
		"create table variant_list (id integer primary key not null)",
		"create index link_index2 on link(to_id)",
		"insert into config values('db_version', 0)",
		"insert into node values(1, 0, 2, -1, -1, '.')",
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

	if(tup_db_begin() < 0)
		return -1;
	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(tup_db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
				errmsg, sql[x]);
			return -1;
		}
	}

	if(tup_db_config_set_int("db_version", DB_VERSION) < 0)
		return -1;
	if(tup_db_config_set_int("parser_version", PARSER_VERSION) < 0)
		return -1;
	if(tup_db_commit() < 0)
		return -1;

	return 0;
}

static int db_backup(int version)
{
	char backup[256];
	int fd;
	int ifd;
	int rc;
	char buf[1024];

	if(snprintf(backup, sizeof(backup), ".tup/db_backup_%i", version) >=
	   (signed)sizeof(backup)) {
		fprintf(stderr, "tup internal error: db backup buffer mis-sized.\n");
		return -1;
	}
	fd = creat(backup, 0666);
	if(fd < 0) {
		perror(backup);
		return -1;
	}
	ifd = open(TUP_DB_FILE, O_RDONLY);
	if(ifd < 0) {
		perror(TUP_DB_FILE);
		goto err_open;
	}
	while(1) {
		rc = read(ifd, buf, sizeof(buf));
		if(rc < 0) {
			perror("read");
			goto err_fail;
		}
		if(rc == 0)
			break;
		if(write(fd, buf, rc) != rc) {
			perror("write");
			goto err_fail;
		}
	}
	if(close(ifd) < 0) {
		perror("close(ifd)");
		return -1;
	}
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}
	printf("Old tup database backed up as '%s'\n", backup);
	return 0;

err_fail:
	close(ifd);
err_open:
	close(fd);
	return -1;
}

static int tup_db_get_tup_config_tent(struct tup_entry **tent)
{
	struct tup_entry *vartent = NULL;
	if(tup_db_select_tent(DOT_DT, TUP_CONFIG, &vartent) < 0) {
		fprintf(stderr, "tup internal error: Unable to check for tup.config node in the project root.\n");
		return -1;
	}
	if(!vartent) {
		vartent = tup_db_create_node(DOT_DT, TUP_CONFIG, TUP_NODE_GHOST);
		if(!vartent) {
			fprintf(stderr, "tup internal error: Unable to create virtual node for tup.config changes in the project root.\n");
			return -1;
		}
	}
	*tent = vartent;
	return 0;
}

static int version_check(void)
{
	int version;
	char *errmsg;
	char sql_parser_update[] = "insert or replace into create_list select id from node where type=2";
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

	char sql_11a[] = "drop index node_dir_index";
	char sql_11b[] = "create table node_new (id integer primary key not null, dir integer not null, type integer not null, sym integer not null, mtime integer not null, name varchar(4096), unique(dir, name))";
	char sql_11c[] = "insert or ignore into node_new select id, dir, type, sym, mtime, name from node";
	char sql_11d[] = "drop index node_sym_index";
	char sql_11e[] = "drop table node";
	char sql_11f[] = "alter table node_new rename to node";
	char sql_11g[] = "create index node_sym_index on node(sym)";

	char sql_12a[] = "create table node_new (id integer primary key not null, dir integer not null, type integer not null, mtime integer not null, name varchar(4096), unique(dir, name))";
	char sql_12b[] = "insert or ignore into node_new select id, dir, type, mtime, name from node";
	char sql_12c[] = "drop index node_sym_index";
	char sql_12d[] = "drop table node";
	char sql_12e[] = "alter table node_new rename to node";

	char sql_13a[] = "create table config_list (id integer primary key not null)";
	char sql_13b[] = "create table variant_list (id integer primary key not null)";
	char sql_13c[] = "alter table node add column srcid integer default -1";
	const char *sql_13d[] = {
		"update node set dir=%lli where dir=2",
		"update create_list set id=%lli where id=2",
		"update modify_list set id=%lli where id=2",
	};
	char *tmpsql;
	struct tup_entry *vartent;
	int x;

	if(tup_db_config_get_int("db_version", -1, &version) < 0)
		return -1;
	if(version < 0) {
		fprintf(stderr, "Error getting .tup/db version.\n");
		return -1;
	}

	if(version > DB_VERSION) {
		fprintf(stderr, "tup error: database is version %i, but this version of tup (%s) can only handle up to %i.\n", version, tup_version(), DB_VERSION);
		return -1;
	}
	if(version != DB_VERSION) {
		printf("Updating tup database from version %i to %i. This may take a while...\n", version, DB_VERSION);
		if(monitor_supported() == 0) {
			/* Monitor is supported (funky return value is because
			 * it can be returned by main).
			 *
			 * Also pass in TUP_MONITOR_SAFE_SHUTDOWN to
			 * stop_monitor() so we don't get an error if there
			 * isn't actually a monitor running. Note that we don't
			 * actually restart the monitor for the user.
			 */
			if(stop_monitor(TUP_MONITOR_SAFE_SHUTDOWN) < 0) {
				fprintf(stderr, "tup error: Unable to stop the monitor during the db version upgrade.\n");
				return -1;
			}
		}
		if(db_backup(version) < 0) {
			fprintf(stderr, "tup error: Unable to backup the current database during the db version upgrade.\n");
			return -1;
		}
	}
	switch(version) {
		case 1:
			if(sqlite3_exec(tup_db, sql_1a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_1a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_1b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_1b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_1c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_1c);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 2) < 0)
				return -1;
			printf("WARNING: Tup database updated to version 2.\nThe link table has a new column (style) to annotate the origin of the link. This is used to differentiate between links specified in Tupfiles vs. links determined automatically via wrapped command execution, so the links can be removed at appropriate times. Also, a new node type (TUP_NODE_GENERATED==4) has been added. All files created from commands have been updated to this new type. This is used so you can't try to create a command to write to a base source file. All Tupfiles will be re-parsed on the next update in order to generate the new links. If you have any problems, it might be easiest to re-checkout your code and start anew. Admittedly I haven't tested the conversion completely.\n");

			fprintf(stderr, "NOTE: If you are using the file monitor, you probably want to restart it.\n");
		case 2:
			if(sqlite3_exec(tup_db, sql_2a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_2a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_2b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_2b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_2c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_2c);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_2d, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_2d);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 3) < 0)
				return -1;
			printf("WARNING: Tup database updated to version 3.\nThe style column in the link table now uses flags instead of multiple records. For example, a link from ID 5 to 7 used to contain 5|7|0 for a normal link and 5|7|1 for a sticky link. Now it is 5|7|1 for a normal link, 5|7|2 for a sticky link, and 5|7|3 for both links.\n");
		case 3:
			if(sqlite3_exec(tup_db, sql_3a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_3a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 4) < 0)
				return -1;
			printf("WARNING: Tup database updated to version 4.\nA 'sym' column has been added to the node table so symlinks can reference their destination nodes. This is necessary in order to properly handle dependencies on symlinks in an efficient manner.\nWARNING: If you have any symlinks in your system, you probably want to delete and re-create them with the monitor running.\n");
		case 4:
			if(sqlite3_exec(tup_db, sql_4a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_4a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_4b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_4b);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 5) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 5.\nThis is a pretty minor update - the link_index is adjusted to use (from_id, to_id) instead of just (from_id). This greatly improves the performance of link insertion, since a query has to be done for uniqueness and style constraints.\n");

		case 5:
			if(sqlite3_exec(tup_db, sql_5a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_5a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 6) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 6.\nAnother minor update - just adding an index on node.sym so it can be quickly determined if a deleted node needs to be made into a ghost.\n");

		case 6:
			if(sqlite3_exec(tup_db, sql_6a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_6a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 7) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 7.\nThis includes a ghost_list for storing ghost ids so they can later be raptured.\n");

		case 7:
			if(sqlite3_exec(tup_db, sql_7a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_7a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 8) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 8.\nThis is really the same as version 6. Turns out putting the ghost_list on disk was kinda stupid. Now it's all handled in a temporary table in memory during a transaction.\n");

		case 8:
			if(sqlite3_exec(tup_db, sql_8a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_8a);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 9) < 0)
				return -1;
			printf("WARNING: Tup database updated to version 9.\nThis version includes a per-file timestamp in order to determine if a file has changed in between monitor invocations, or during a scan. You will want to restart the monitor in order to set the mtime field for all the files. Note that since no mtimes currently exist in the database, this will cause all commands to be executed for the next update.\n");

		case 9:
			if(sqlite3_exec(tup_db, sql_9a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9c);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9d, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9d);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9e, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9e);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9f, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9f);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_9g, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_9g);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 10) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 10.\nA new unique constraint was placed on the link table.\n");

		case 10:
			if(sqlite3_exec(tup_db, sql_10, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_10);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 11) < 0)
				return -1;
			printf("NOTE: This database goes to 11.\nThe delete_list is no longer necessary, and is now gone.\n");

		case 11:
			/* First clear out any ghosts that shouldn't be there.
			 * We don't want them to take precedence over real
			 * nodes that may have been moved over them.
			 */
			if(tup_db_debug_add_all_ghosts() < 0)
				return -1;
			if(tup_entry_clear() < 0)
				return -1;
			if(sqlite3_exec(tup_db, sql_11a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_11b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_11c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11c);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_11d, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11d);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_11e, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11e);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_11f, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11f);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_11g, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_11g);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 12) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 12.\nExtraneous ghosts were removed, and a new unique constraint was placed on the node table.\n");

		case 12:
			if(sqlite3_exec(tup_db, sql_12a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_12a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_12b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_12b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_12c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_12c);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_12d, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_12d);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_12e, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_12e);
				return -1;
			}
			if(tup_db_config_set_int("db_version", 13) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 13.\nThe sym field was removed, since symlinks are automatically handled by the filesystem layer.\n");

		case 13:
			if(sqlite3_exec(tup_db, sql_13a, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_13a);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_13b, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_13b);
				return -1;
			}
			if(sqlite3_exec(tup_db, sql_13c, NULL, NULL, &errmsg) != 0) {
				fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
					errmsg, sql_13c);
				return -1;
			}
			if(tup_db_get_tup_config_tent(&vartent) < 0)
				return -1;
			/* Move all @-variable nodes over to have ./tup.config
			 * as the parent
			 */
			for(x=0; x<ARRAY_SIZE(sql_13d); x++) {
				tmpsql = sqlite3_mprintf(sql_13d[x], vartent->tnode.tupid);
				if(!tmpsql) {
					fprintf(stderr, "tup error: Unable to allocate memory for database upgrade SQL query.\n");
					return -1;
				}
				if(sqlite3_exec(tup_db, tmpsql, NULL, NULL, &errmsg) != 0) {
					fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
						errmsg, tmpsql);
					return -1;
				}
				sqlite3_free(tmpsql);
			}
			if(delete_name_file(2) < 0)
				return -1;

			if(tup_db_config_set_int("db_version", 14) < 0)
				return -1;
			printf("NOTE: Tup database updated to version 14.\nA new config_list table was added to handle tup.config changes for variants.\n");

			/***************************************/
			/* Last case must fall through to here */
			if(tup_db_commit() < 0)
				return -1;
			if(tup_db_begin() < 0)
				return -1;
			printf("Database update successful. You can remove the backup database file in the .tup/ directory if everything appears to be working.\n");
		case DB_VERSION:
			break;
		default:
			fprintf(stderr, "tup error: Unable to convert database version %i to version %i\n", version, DB_VERSION);
			return -1;
	}

	if(tup_db_config_get_int("parser_version", 0, &version) < 0)
		return -1;
	if(version < 0) {
		fprintf(stderr, "Error getting tup parser version.\n");
		return -1;
	}
	if(version != PARSER_VERSION) {
		printf("Tup parser version has been updated to %i. All Tupfiles will be re-parsed to ensure that nothing broke.\n", PARSER_VERSION);
		if(sqlite3_exec(tup_db, sql_parser_update, NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
				errmsg, sql_parser_update);
			return -1;
		}
		if(tup_db_unflag_create(env_dt()) < 0)
			return -1;
		if(tup_db_config_set_int("parser_version", PARSER_VERSION) < 0)
			return -1;
	}

	return 0;
}

int tup_db_begin(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_BEGIN];
	static char s[] = "begin";

	transaction = 1;
	transaction_check("%s", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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

	transaction_check("%s", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	transaction = 0;
	return 0;
}

int tup_db_rollback(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ROLLBACK];
	static char s[] = "rollback";

	transaction_check("%s", s);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			return -1;
		}
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	return 0;
}

static const char *check_flags_name;
static int check_flags_cb(void *arg, int argc, char **argv, char **col)
{
	int *iptr = arg;
	if(col) {}
	if(check_flags_name) {
		fprintf(stderr, "*** %s_list:\n", check_flags_name);
		check_flags_name = NULL;
	}
	if(argc >= 1 && argv) {
		fprintf(stderr, "%s\n", argv[0]);
	}
	*iptr = 1;
	return 0;
}

int tup_db_check_flags(int flags)
{
	int rc = 0;
	char *errmsg;
	char s1[] = "select * from config_list";
	char s2[] = "select * from create_list";
	char s3[] = "select * from modify_list";

	if(tup_db_begin() < 0)
		return -1;
	if(flags & TUP_FLAGS_CONFIG) {
		transaction_check("%s", s1);
		check_flags_name = "config";
		if(sqlite3_exec(tup_db, s1, check_flags_cb, &rc, &errmsg) != 0) {
			fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
				errmsg, s1);
			sqlite3_free(errmsg);
			return -1;
		}
	}
	if(flags & TUP_FLAGS_CREATE) {
		transaction_check("%s", s2);
		check_flags_name = "create";
		if(sqlite3_exec(tup_db, s2, check_flags_cb, &rc, &errmsg) != 0) {
			fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
				errmsg, s2);
			sqlite3_free(errmsg);
			return -1;
		}
	}
	if(flags & TUP_FLAGS_MODIFY) {
		transaction_check("%s", s3);
		check_flags_name = "modify";
		if(sqlite3_exec(tup_db, s3, check_flags_cb, &rc, &errmsg) != 0) {
			fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
				errmsg, s3);
			sqlite3_free(errmsg);
			return -1;
		}
	}
	if(tup_db_commit() < 0)
		return -1;
	return rc;
}

void tup_db_enable_sql_debug(void)
{
	sql_debug = 1;
}

int tup_db_debug_add_all_ghosts(void)
{
	reclaim_ghost_debug = 1;

	/* First get all tup_entrys loaded */
	if(files_to_tree(NULL) < 0)
		return -1;

	if(tup_entry_debug_add_all_ghosts(&ghost_list) < 0)
		return -1;

	return 0;
}

const char *tup_db_type(enum TUP_NODE_TYPE type)
{
	const char *str;
	switch(type) {
		case TUP_NODE_FILE:
			str = "normal file";
			break;
		case TUP_NODE_CMD:
			str = "command";
			break;
		case TUP_NODE_DIR:
			str = "directory";
			break;
		case TUP_NODE_VAR:
			str = "@-variable";
			break;
		case TUP_NODE_GENERATED:
			str = "generated file";
			break;
		case TUP_NODE_GHOST:
			str = "ghost";
			break;
		case TUP_NODE_ROOT:
			str = "graph root";
			break;
		default:
			str = "unknown";
			break;
	}
	return str;
}

struct tup_entry *tup_db_create_node(tupid_t dt, const char *name, int type)
{
	return tup_db_create_node_part(dt, name, -1, type);
}

struct tup_entry *tup_db_create_node_part(tupid_t dt, const char *name, int len,
					  int type)
{
	struct tup_entry *tent;

	if(node_select(dt, name, len, &tent) < 0) {
		return NULL;
	}

	if(tent) {
		if(tent->type == TUP_NODE_GHOST) {
			if(type == TUP_NODE_VAR) {
				if(tup_db_add_create_list(tent->tnode.tupid) < 0)
					return NULL;
			} else if(type == TUP_NODE_GENERATED) {
				/* t6046, t6047 - when a ghost is upgraded to a
				 * generated node, we must effectively remove
				 * the ghost from the DAG (flag all pointed-to
				 * nodes as necessary and delete all the
				 * links). The reason is if we have:
				 *
				 * ghost |> cmdA |>...
				 *
				 * then later we do
				 *
				 * |> cmdB |> ghost
				 *
				 * and try to upgrade the ghost to a generated
				 * node, we will have bypassed the checking in
				 * cmdA for a deleted node in the input list.
				 */
				if(tup_db_set_dependent_dir_flags(tent->tnode.tupid) < 0)
					return NULL;
				if(tup_db_modify_cmds_by_input(tent->tnode.tupid) < 0)
					return NULL;
				if(tup_db_delete_links(tent->tnode.tupid) < 0)
					return NULL;
			}
			if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
				return NULL;
			if(tup_db_set_type(tent, type) < 0)
				return NULL;
			return tent;
		}
		if(tent->type != type) {
			/* Try to provide a more sane error message in this
			 * case, since a user might come across it just by
			 * screwing up the Tupfile.
			 */
			if(type == TUP_NODE_GENERATED) {
				fprintf(stderr, "tup error: Attempting to insert '%s' as a generated node when it already exists as a different type (%s). You can do one of two things to fix this:\n  1) If this file is really supposed to be created from the command, delete the file from the filesystem and try again.\n  2) Change your rule in the Tupfile so you aren't trying to overwrite the file.\n", name, tup_db_type(tent->type));
				return NULL;
			}

			/* If we changed from one type to another (eg: a file
			 * became a directory), then delete the old one and
			 * create a new one.
			 */
			if(tup_del_id_force(tent->tnode.tupid, tent->type) < 0)
				return NULL;
			goto out_create;
		}
		return tent;
	}

out_create:
	tent = tup_db_node_insert(dt, name, len, type, -1);
	return tent;
}

int tup_db_fill_tup_entry(tupid_t tupid, struct tup_entry *tent)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_FILL_TUP_ENTRY];
	static char s[] = "select dir, type, mtime, srcid, name from node where id=?";
	const char *name;
	int len;

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr, "tup error: Unable to find node entry for tupid: %lli\n", tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}

	tent->dt = sqlite3_column_int64(*stmt, 0);
	tent->type = sqlite3_column_int(*stmt, 1);
	tent->mtime = sqlite3_column_int(*stmt, 2);
	tent->srcid = sqlite3_column_int64(*stmt, 3);
	name = (const char*)sqlite3_column_text(*stmt, 4);
	len = strlen(name);
	tent->name.s = malloc(len + 1);
	if(!tent->name.s) {
		perror("malloc");
		goto out_reset;
	}
	strcpy(tent->name.s, name);
	tent->name.len = len;
	rc = 0;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_select_tent(tupid_t dt, const char *name, struct tup_entry **entry)
{
	return node_select(dt, name, -1, entry);
}

int tup_db_select_tent_part(tupid_t dt, const char *name, int len,
			    struct tup_entry **entry)
{
	return node_select(dt, name, len, entry);
}

int tup_db_select_node_by_flags(int (*callback)(void *, struct tup_entry *,
						int style),
				void *arg, int flags)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt;
	static char s1[] = "select * from config_list";
	static char s2[] = "select * from create_list";
	static char s3[] = "select * from modify_list";
	char *sql;
	int sqlsize;

	if(flags == TUP_FLAGS_CONFIG) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_1];
		sql = s1;
		sqlsize = sizeof(s1);
	} else if(flags == TUP_FLAGS_CREATE) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_2];
		sql = s2;
		sqlsize = sizeof(s2);
	} else if(flags == TUP_FLAGS_MODIFY) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_3];
		sql = s3;
		sqlsize = sizeof(s3);
	} else {
		fprintf(stderr, "tup error: tup_db_select_node_by_flags() must specify exactly one of TUP_FLAGS_CONFIG/TUP_FLAGS_CREATE/TUP_FLAGS_MODIFY\n");
		return -1;
	}

	transaction_check("%s", sql);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, sql, sqlsize, stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", sql);
			return -1;
		}
	}

	while(1) {
		struct tup_entry *tent;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", sql);
			rc = -1;
			goto out_reset;
		}

		if(tup_entry_add(sqlite3_column_int64(*stmt, 0), &tent) < 0) {
			rc = -1;
			goto out_reset;
		}

		/* Since this is used to build the initial part of the DAG,
		 * we use TUP_LINK_NORMAL so the nodes that are returned will
		 * be expanded.
		 */
		if((rc = callback(arg, tent, TUP_LINK_NORMAL)) < 0) {
			goto out_reset;
		}
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	return rc;
}

int tup_db_select_node_dir(int (*callback)(void *, struct tup_entry *, int style),
			   void *arg, tupid_t dt)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_DIR];
	static char s[] = "select id from node where dir=?";

	transaction_check("%s [37m[%lli][0m", s, dt);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		struct tup_entry *tent;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		if(tup_entry_add(sqlite3_column_int64(*stmt, 0), &tent) < 0) {
			rc = -1;
			goto out_reset;
		}

		/* This is used by the 'tup graph' function if the user wants to
		 * graph a directory. Since we want to expand all nodes in the
		 * directory, we use TUP_LINK_NORMAL.
		 */
		if(callback(arg, tent, TUP_LINK_NORMAL) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_select_node_dir_glob(int (*callback)(void *, struct tup_entry *),
				void *arg, tupid_t dt, const char *glob,
				int len, struct tupid_entries *delete_root)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_DIR_GLOB];
	static char s[] = "select id, name, type, mtime, srcid from node where dir=? and (type=? or type=?) and name glob ?" SQL_NAME_COLLATION;

	transaction_check("%s [37m[%lli, %i, %i, '%s'][0m", s, dt, TUP_NODE_FILE, TUP_NODE_GENERATED, glob);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_FILE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 4, glob, len, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		struct tup_entry *tent;
		tupid_t tupid;
		const char *name;
		int type;
		time_t mtime;
		tupid_t srcid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		tupid = sqlite3_column_int64(*stmt, 0);
		if(tupid_tree_search(delete_root, tupid) == NULL) {
			tent = tup_entry_find(tupid);
			if(!tent) {
				name = (const char *)sqlite3_column_text(*stmt, 1);
				type = sqlite3_column_int(*stmt, 2);
				mtime = sqlite3_column_int64(*stmt, 3);
				srcid = sqlite3_column_int64(*stmt, 4);

				if(tup_entry_add_to_dir(dt, tupid, name, -1, type, mtime, srcid, &tent) < 0) {
					rc = -1;
					goto out_reset;
				}
			}

			if(callback(arg, tent) < 0) {
				rc = -1;
				goto out_reset;
			}
		}
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_delete_node(tupid_t tupid)
{
	int rc;

	rc = node_has_ghosts(tupid);
	if(rc < 0)
		return -1;
	if(rc == 1) {
		/* We're but a ghost now. This can happen if a directory is
		 * removed that contains a ghost (t6061).
		 */
		struct tup_entry *tent;

		if(tup_entry_add(tupid, &tent) < 0)
			return -1;
		if(tup_db_set_type(tent, TUP_NODE_GHOST) < 0)
			return -1;
		return 0;
	}

	return delete_node(tupid);
}

int delete_node(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_NODE];
	static char s[] = "delete from node where id=?";

	if(tup_entry_rm(tupid) < 0) {
		return -1;
	}

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_delete_dir(tupid_t dt)
{
	struct half_entry_head subdir_list;

	LIST_INIT(&subdir_list);
	if(get_dir_entries(dt, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);

		/* Don't want to delete ghosts, since they may still link to
		 * somewhere useful (t6061)
		 */
		if(he->type != TUP_NODE_GHOST) {
			/* tup_del_id_force may call back to tup_db_delete_dir() */
			if(tup_del_id_force(he->tupid, he->type) < 0)
				return -1;
		}
		LIST_REMOVE(he, list);
		free(he);
	}

	return 0;
}

static int recurse_delete_ghost_tree(tupid_t tupid, int modify)
{
	struct half_entry *he;
	struct half_entry_head subdir_list;

	LIST_INIT(&subdir_list);
	if(get_dir_entries(tupid, &subdir_list) < 0)
		return -1;

	/* This is a subset of tup_del_id() that we need for ghost files. Note
	 * that we also don't call tup_db_delete_node() directly because that
	 * makes a check to see if we need to convert the node into a ghost.
	 * Since we already know the node is going to become a regular
	 * directory or file, we don't want to do that check.
	 *
	 * Since ghost nodes can have sub-ghosts, we have to recurse and
	 * delete them all.
	 *
	 * Note that we don't have to do anything for the create list here. The
	 * only ghosts that affect create nodes are Tuprules.tup files, and you
	 * can't have an existing Tupfile that is a subdirectory to a ghost
	 * directory.
	 */
	if(modify)
		if(tup_db_modify_cmds_by_input(tupid) < 0)
			return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;

	LIST_FOREACH(he, &subdir_list, list) {
		if(he->type != TUP_NODE_GHOST) {
			fprintf(stderr, "tup internal error: Why does a node of type %i have a ghost dir?\n", he->type);
			tup_db_print(stderr, he->tupid);
			return -1;
		}
		if(recurse_delete_ghost_tree(he->tupid, modify) < 0)
			return -1;
	}
	if(delete_node(tupid) < 0)
		return -1;
	return 0;
}

int tup_db_modify_dir(tupid_t dt)
{
	struct id_entry_head subdir_list;
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MODIFY_DIR];
	static char s[] = "insert or ignore into modify_list select id from node where dir=? and type!=?";

	if(tup_db_add_create_list(dt) < 0)
		return -1;
	transaction_check("%s [37m[%lli, %i][0m", s, dt, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	LIST_INIT(&subdir_list);
	if(get_recurse_dirs(dt, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct id_entry *ide = LIST_FIRST(&subdir_list);
		tup_db_modify_dir(ide->tupid);
		LIST_REMOVE(ide, list);
		free(ide);
	}

	return 0;
}

int tup_db_get_generated_tup_entries(tupid_t dt, struct tup_entry_head *head)
{
	struct tupid_entries dir_entries;
	struct id_entry_head subdir_list;
	struct tupid_tree *tt;

	RB_INIT(&dir_entries);
	if(tup_db_dirtype_to_tree(dt, &dir_entries, NULL, TUP_NODE_GENERATED) < 0)
		return -1;
	while((tt = RB_ROOT(&dir_entries)) != NULL) {
		struct tup_entry *subtent;
		if(tup_entry_add(tt->tupid, &subtent) < 0)
			return -1;
		tupid_tree_rm(&dir_entries, tt);
		free(tt);
		tup_entry_list_add(subtent, head);
	}

	LIST_INIT(&subdir_list);
	if(get_recurse_dirs(dt, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct id_entry *ide = LIST_FIRST(&subdir_list);
		if(tup_db_get_generated_tup_entries(ide->tupid, head) < 0)
			return -1;
		LIST_REMOVE(ide, list);
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
		fprintf(stderr, "tup error: Trying to tup_db_open_tupid(0)\n");
		return -1;
	}
	if(tupid == 1) {
		return dup(tup_top_fd());
	}
	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -ENOENT;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		free(path);
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
	if(close(fd) < 0) {
		perror("close(fd)");
		rc = -1;
	}
	free(path);

	return rc;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_change_node(tupid_t tupid, const char *new_name, tupid_t new_dt)
{
	int rc;
	struct tup_entry *tent;
	sqlite3_stmt **stmt = &stmts[DB_CHANGE_NODE_NAME];
	static char s[] = "update node set name=?, dir=? where id=?";

	if(node_select(new_dt, new_name, -1, &tent) < 0) {
		return -1;
	}
	if(tent) {
		if(tent->type == TUP_NODE_GHOST) {
			if(recurse_delete_ghost_tree(tent->tnode.tupid, 1) < 0)
				return -1;
		} else {
			fprintf(stderr, "tup error: Attempting to overwrite node '%s' in dir %lli in tup_db_change_node()\n", new_name, new_dt);
			tup_db_print(stderr, new_dt);
			return -1;
		}
	}

	transaction_check("%s [37m['%s', %lli, %lli][0m", s, new_name, new_dt, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, new_name, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, new_dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 3, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(tup_entry_change_name_dt(tupid, new_name, new_dt) < 0)
		return -1;

	return 0;
}

int tup_db_set_name(tupid_t tupid, const char *new_name)
{
	int rc;
	struct tup_entry *tent;
	sqlite3_stmt **stmt = &stmts[DB_SET_NAME];
	static char s[] = "update node set name=? where id=?";

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	rc = strcmp(tent->name.s, new_name);
	if(rc == 0) {
		return 0;
	}

	transaction_check("%s [37m['%s', %lli][0m", s, new_name, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, new_name, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(tup_entry_change_name(tupid, new_name) < 0)
		return -1;

	/* Since we changed the name, we have to run the command again. */
	if(tup_db_add_modify_list(tupid) < 0)
		return -1;

	return 0;
}

int tup_db_set_type(struct tup_entry *tent, int type)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_TYPE];
	static char s[] = "update node set type=? where id=?";

	transaction_check("%s [37m[%i, %lli][0m", s, type, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tent->tnode.tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	tent->type = type;
	return 0;
}

int tup_db_set_mtime(struct tup_entry *tent, time_t mtime)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_MTIME];
	static char s[] = "update node set mtime=? where id=?";

	transaction_check("%s [37m[%li, %lli][0m", s, mtime, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, mtime) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tent->tnode.tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	tent->mtime = mtime;
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
				    struct tupid_entries *root)
{
	int alloc_len;

	*s = NULL;
	*len = 0;
	alloc_len = generated_nodelist_len(dt);
	if(alloc_len < 0)
		return -1;
	if(alloc_len == 0)
		return 0;
	/* The length may be an over-estimate, since it also contains any
	 * nodes scheduled to be deleted.
	 */
	*s = calloc(alloc_len, 1);
	if(!*s) {
		perror("calloc");
		return -1;
	}
	if(get_generated_nodelist(*s, dt, root, len) < 0)
		return -1;
	return 0;
}

int tup_db_rebuild_all(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_REBUILD_ALL];
	static char s[] = "insert or ignore into modify_list select id from node where type=?";

	transaction_check("%s [37m[%i][0m", s, TUP_NODE_CMD);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_CMD) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_delete_slash(void)
{
	tupid_t dt = slash_dt();
	if(recurse_delete_ghost_tree(dt, 0) < 0)
		return -1;
	local_slash_dt = -1;
	return 0;
}

static int generated_nodelist_len(tupid_t dt)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODELIST_LEN];
	static char s[] = "select sum(length(name) + 2) from node where dir=? and type=?";

	transaction_check("%s [37m[%lli, %i][0m", s, dt, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}

	rc = sqlite3_column_int(*stmt, 0);

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int get_generated_nodelist(char *dest, tupid_t dt,
				  struct tupid_entries *root, int *total_len)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_NODELIST];
	static char s[] = "select length(name), name, id from node where dir=? and type=?";
	char *p;
	int len;

	transaction_check("%s [37m[%lli, %i][0m", s, dt, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}
		if(tupid_tree_search(root, sqlite3_column_int64(*stmt, 2)) != NULL) {
			continue;
		}
		*p = PATH_SEP;
		p++;
		len = sqlite3_column_int(*stmt, 0);
		memcpy(p, sqlite3_column_text(*stmt, 1), len);
		(*total_len) += len + 2;
		p += len;
		*p = '\n';
		p++;
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
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
		fprintf(stderr, "tup error: Trying to tup_db_print(0)\n");
		return -1;
	}
	if(tupid == 1) {
		return 0;
	}
	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = -ENOENT;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		free(path);
		return -1;
	}

	if(db_print(stream, parent) < 0) {
		free(path);
		return -1;
	}

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
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_add_config_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_CONFIG_LIST];
	static char s[] = "insert or ignore into config_list values(?)";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_add_create_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_CREATE_LIST];
	static char s[] = "insert or ignore into create_list values(?)";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_add_modify_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_MODIFY_LIST];
	static char s[] = "insert or ignore into modify_list values(?)";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_add_variant_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_VARIANT_LIST];
	static char s[] = "insert or ignore into variant_list values(?)";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_in_config_list(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_IN_CONFIG_LIST];
	static char s[] = "select id from config_list where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}

	rc = 1;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_in_create_list(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_IN_CREATE_LIST];
	static char s[] = "select id from create_list where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}

	rc = 1;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}

	rc = 1;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_unflag_config(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_CONFIG];
	static char s[] = "delete from config_list where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_unflag_create(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_CREATE];
	static char s[] = "delete from create_list where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_unflag_modify(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_MODIFY];
	static char s[] = "delete from modify_list where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_unflag_variant(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_VARIANT];
	static char s[] = "delete from variant_list where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static int get_recurse_dirs(tupid_t dt, struct id_entry_head *head)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_RECURSE_DIRS];
	static char s[] = "select id from node where dir=? and type=?";

	transaction_check("%s [37m[%lli, %i][0m", s, dt, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		ide = malloc(sizeof *ide);
		if(ide == NULL) {
			perror("malloc");
			rc = -1;
			goto out_reset;
		}
		ide->tupid = sqlite3_column_int64(*stmt, 0);
		LIST_INSERT_HEAD(head, ide, list);
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int get_dir_entries(tupid_t dt, struct half_entry_head *head)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_DIR_ENTRIES];
	static char s[] = "select id, type from node where dir=?";

	transaction_check("%s [37m[%lli][0m", s, dt);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		struct half_entry *he;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		he = malloc(sizeof *he);
		if(he == NULL) {
			perror("malloc");
			rc = -1;
			goto out_reset;
		}
		he->tupid = sqlite3_column_int64(*stmt, 0);
		he->dt = dt;
		he->type = sqlite3_column_int(*stmt, 1);
		LIST_INSERT_HEAD(head, he, list);
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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

int tup_db_create_unique_link(tupid_t a, tupid_t b, struct tupid_entries *delroot,
			      struct tupid_entries *root)
{
	int rc;
	tupid_t incoming;

	rc = tup_db_get_incoming_link(b, &incoming);
	if(rc < 0)
		return -1;
	if(incoming != -1) {
		if(tupid_tree_search(delroot, incoming) != NULL) {
			/* Delete any old links (t6029) */
			if(link_remove(incoming, b) < 0)
				return -1;
			incoming = -1;
		}
	}
	/* See if we already own the link, or if the link doesn't exist yet */
	if(a == incoming || incoming == -1) {
		if(tupid_tree_add(root, b) < 0)
			return -1;
		return 0;
	}
	/* Otherwise, someone else got the girl. Err, output file. */
	fprintf(stderr, "tup error: Unable to create a unique link from %lli to %lli because the destination is already linked to by node %lli.\n", a, b, incoming);
	tup_db_print(stderr, a);
	tup_db_print(stderr, b);
	return -1;
}

int tup_db_link_exists(tupid_t a, tupid_t b)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_LINK_EXISTS];
	static char s[] = "select to_id from link where from_id=? and to_id=?";

	transaction_check("%s [37m[%lli, %lli][0m", s, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc == SQLITE_DONE) {
		return -1;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_link_style(tupid_t a, tupid_t b, int *style)
{
	int rc = -1;
	sqlite3_stmt **stmt = &stmts[DB_LINK_STYLE];
	static char s[] = "select style from link where from_id=? and to_id=?";

	*style = -1;

	transaction_check("%s [37m[%lli, %lli][0m", s, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}
	*style = sqlite3_column_int(*stmt, 0);
	rc = 0;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_get_incoming_link(tupid_t tupid, tupid_t *incoming)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_GET_INCOMING_LINK];
	static char s[] = "select from_id from link where to_id=?";

	*incoming = -1;

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}
	*incoming = sqlite3_column_int64(*stmt, 0);

	/* Do a quick double-check to make sure there isn't a duplicate link. */
	dbrc = sqlite3_step(*stmt);
	if(dbrc != SQLITE_DONE) {
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
		fprintf(stderr, "tup error: Node %lli is supposed to only have one incoming link, but multiple were found. The database is probably in a bad state. Sadness :(\n", tupid);
		rc = -1;
		goto out_reset;
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_delete_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_LINKS];
	static char s[] = "delete from link where from_id=? or to_id=?";

	if(add_ghost_links(tupid) < 0)
		return -1;

	transaction_check("%s [37m[%lli, %lli][0m", s, tupid, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_dirtype_to_tree(tupid_t dt, struct tupid_entries *root, int *count, int type)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_DIRTYPE_TO_TREE];
	static char s[] = "select id from node where dir=? and type=?";

	transaction_check("%s [37m[%lli, %i][0m", s, dt, type);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		tupid_t tupid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			break;
		}

		tupid = sqlite3_column_int64(*stmt, 0);

		if(tree_entry_add(root, tupid, type, count) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_modify_cmds_by_output(tupid_t output, int *modified)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MODIFY_CMDS_BY_OUTPUT];
	static char s[] = "insert or ignore into modify_list select from_id from link where to_id=?";

	transaction_check("%s [37m[%lli][0m", s, output);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, output) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
	static char s[] = "insert or ignore into modify_list select to_id from link, node where from_id=? and to_id=id and type=?";

	transaction_check("%s [37m[%lli, %i][0m", s, input, TUP_NODE_CMD);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, input) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_CMD) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_set_dependent_dir_flags(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_DEPENDENT_DIR_FLAGS];
	static char s[] = "insert or ignore into create_list select to_id from link, node where from_id=? and to_id=id and type=?";

	transaction_check("%s [37m[%lli, %i][0m", s, tupid, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_select_node_by_link(int (*callback)(void *, struct tup_entry *,
					       int style),
			       void *arg, tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_BY_LINK];
	static char s[] = "select to_id, style from link where from_id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		struct tup_entry *tent;
		int style;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		if(tup_entry_add(sqlite3_column_int64(*stmt, 0), &tent) < 0) {
			rc = -1;
			goto out_reset;
		}
		style = sqlite3_column_int(*stmt, 1);

		if(callback(arg, tent, style) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
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
		fprintf(stderr, "SQL select error: %s\n", errmsg);
		fprintf(stderr, "Statement was: %s\n", s);
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

	transaction_check("%s [37m['%s', %i][0m", s, lval, x);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, x) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_config_get_int(const char *lval, int def, int *result)
{
	int rc = -1;
	int dbrc;
	int set_default = 0;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_GET_INT];
	static char s[] = "select rval from config where lval=?";

	transaction_check("%s [37m['%s'][0m", s, lval);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		set_default = 1;
		*result = def;
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}

	*result = sqlite3_column_int(*stmt, 0);
	rc = 0;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(set_default) {
		if(tup_db_config_set_int(lval, def) < 0)
			return -1;
	}

	return rc;
}

int tup_db_config_set_string(const char *lval, const char *rval)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_CONFIG_SET_STRING];
	static char s[] = "insert or replace into config values(?, ?)";

	transaction_check("%s [37m['%s', '%s'][0m", s, lval, rval);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, lval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, rval, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

int tup_db_set_var(tupid_t tupid, const char *value)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_VAR];
	static char s[] = "insert or replace into var values(?, ?)";

	transaction_check("%s [37m[%lli, '%s'][0m", s, tupid, value);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, value, -1, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static struct var_entry *get_var_id(struct tup_entry *tent,
				    const char *var, int varlen)
{
	struct var_entry *ve = NULL;
	int dbrc;
	int len;
	const char *value;
	sqlite3_stmt **stmt = &stmts[_DB_GET_VAR_ID];
	static char s[] = "select value, length(value) from var where var.id=?";

	transaction_check("%s [37m[%lli][0m", s, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return NULL;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tent->tnode.tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return NULL;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"tup error: Variable id %lli not found in .tup/db.\n", tent->tnode.tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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

	ve = vardb_set2(&atvardb, var, varlen, value, tent);

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return NULL;
	}

	return ve;
}

static struct var_entry *get_var(tupid_t vardt, const char *var, int varlen)
{
	struct var_entry *ve;

	ve = vardb_get(&atvardb, var, varlen);
	if(!ve) {
		struct tup_entry *tent;
		struct tup_entry *var_tent;

		if(tup_entry_add(vardt, &var_tent) < 0)
			return NULL;
		if(node_select(var_tent->tnode.tupid, var, varlen, &tent) < 0)
			return NULL;
		if(!tent) {
			tent = tup_db_node_insert(vardt, var, varlen,
						  TUP_NODE_GHOST, -1);
			if(!tent)
				return NULL;
		}
		if(tent->type == TUP_NODE_VAR) {
			ve = get_var_id(tent, var, varlen);
		} else if(varlen == 12 && strncmp(var, "TUP_PLATFORM", varlen) == 0) {
			ve = vardb_set2(&atvardb, var, varlen, tup_platform, tent);
		} else if(varlen == 8 && strncmp(var, "TUP_ARCH", varlen) == 0) {
			ve = vardb_set2(&atvardb, var, varlen, tup_arch, tent);
		} else {
			ve = vardb_set2(&atvardb, var, varlen, "", tent);
		}
	}
	return ve;
}

struct tup_entry *tup_db_get_var(tupid_t vardt, const char *var, int varlen, char **dest)
{
	struct var_entry *ve;

	ve = get_var(vardt, var, varlen);
	if(!ve)
		return NULL;

	if(dest) {
		memcpy(*dest, ve->value, ve->vallen);
		*dest += ve->vallen;
	}
	return ve->tent;
}

int tup_db_get_var_id_alloc(tupid_t tupid, char **dest)
{
	int rc = -1;
	int dbrc;
	int len;
	const char *value;
	sqlite3_stmt **stmt = &stmts[DB_GET_VAR_ID_ALLOC];
	static char s[] = "select value, length(value) from var where var.id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		fprintf(stderr,"tup error: Variable id %lli not found in .tup/db.\n", tupid);
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_get_varlen(tupid_t vardt, const char *var, int varlen)
{
	struct var_entry *ve;

	ve = get_var(vardt, var, varlen);
	if(!ve)
		return -1;
	return ve->vallen;
}

int tup_db_var_foreach(tupid_t dt, int (*callback)(void *, tupid_t tupid, const char *var, const char *value, int type), void *arg)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_VAR_FOREACH];
	static char s[] = "select node.id, name, value, type from var, node where node.dir=? and node.id=var.id order by name" SQL_NAME_COLLATION;

	transaction_check("%s [37m[%lli][0m", s, dt);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		const char *var;
		const char *value;
		int type;
		tupid_t tupid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		tupid = sqlite3_column_int64(*stmt, 0);
		var = (const char *)sqlite3_column_text(*stmt, 1);
		value = (const char *)sqlite3_column_text(*stmt, 2);
		type = sqlite3_column_int(*stmt, 3);

		if((rc = callback(arg, tupid, var, value, type)) < 0) {
			goto out_reset;
		}
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int save_vardict_file(struct vardb *vdb)
{
	int dfd;
	int fd;
	int rc = -1;
	unsigned int x;
	struct string_tree *st;

	if(tup_db_var_changed == 0)
		return 0;

	dfd = tup_db_open_tupid(DOT_DT);
	if(dfd < 0) {
		return -1;
	}
	fd = openat(dfd, TUP_VARDICT_FILE, O_CREAT|O_WRONLY|O_TRUNC, 0666);
	if(fd < 0) {
		perror("openat");
		return -1;
	}
	if(close(dfd) < 0) {
		perror("close(dfd)");
		return -1;
	}
	if(write(fd, &vdb->count, sizeof(vdb->count)) != sizeof(vdb->count)) {
		perror("write");
		goto out_err;
	}
	/* Write out index */
	x = 0;
	RB_FOREACH(st, string_entries, &vdb->root) {
		struct var_entry *ve;
		ve = container_of(st, struct var_entry, var);
		if(write(fd, &x, sizeof(x)) != sizeof(x)) {
			perror("write");
			goto out_err;
		}
		/* each line is 'variable=value', so +1 for the equals sign,
		 * and +1 for the newline.
		 */
		x += st->len + 1 + ve->vallen + 1;
	}

	/* Write out the variables */
	RB_FOREACH(st, string_entries, &vdb->root) {
		struct var_entry *ve;
		ve = container_of(st, struct var_entry, var);

		if(write(fd, st->s, st->len) != st->len) {
			perror("write");
			goto out_err;
		}
		if(write(fd, "=", 1) != 1) {
			perror("write");
			goto out_err;
		}
		if(write(fd, ve->value, ve->vallen) != ve->vallen) {
			perror("write");
			goto out_err;
		}
		if(write(fd, "\0", 1) != 1) {
			perror("write");
			goto out_err;
		}
	}

	rc = 0;
	tup_db_var_changed = 0;
out_err:
	if(close(fd) < 0) {
		perror("close(fd)");
		rc = -1;
	}
	return rc;
}

static int remove_var(struct var_entry *ve, tupid_t vardt)
{
	if(vardt) {}

	tup_db_var_changed++;

	if(var_flag_dirs(ve->tent->tnode.tupid) < 0)
		return -1;
	if(tup_db_modify_cmds_by_input(ve->tent->tnode.tupid) < 0)
		return -1;
	if(delete_var_entry(ve->tent->tnode.tupid) < 0)
		return -1;
	if(delete_name_file(ve->tent->tnode.tupid) < 0)
		return -1;
	return 0;
}

static int add_var(struct var_entry *ve, tupid_t vardt)
{
	struct tup_entry *tent;
	struct tup_entry *var_tent;

	tup_db_var_changed++;

	if(tup_entry_add(vardt, &var_tent) < 0)
		return -1;
	tent = tup_db_create_node(var_tent->tnode.tupid, ve->var.s, TUP_NODE_VAR);
	if(!tent)
		return -1;
	return tup_db_set_var(tent->tnode.tupid, ve->value);
}

static int compare_vars(struct var_entry *vea, struct var_entry *veb)
{
	struct tup_entry *tent;

	if(vea->tent->type == TUP_NODE_VAR && vea->vallen == veb->vallen &&
	   strcmp(vea->value, veb->value) == 0) {
		return 0;
	}
	tup_db_var_changed++;
	if(tup_entry_add(vea->tent->tnode.tupid, &tent) < 0)
		return -1;
	if(tup_db_add_create_list(vea->tent->tnode.tupid) < 0)
		return -1;
	if(tup_db_add_modify_list(vea->tent->tnode.tupid) < 0)
		return -1;
	return tup_db_set_var(vea->tent->tnode.tupid, veb->value);
}

int tup_db_read_vars(tupid_t dt, const char *file, tupid_t vardt)
{
	struct vardb db_tree;
	struct vardb file_tree;
	int dfd;
	int fd;
	int rc;

	vardb_init(&db_tree);
	vardb_init(&file_tree);
	if(get_db_var_tree(vardt, &db_tree) < 0)
		return -1;
	dfd = tup_db_open_tupid(dt);
	if(dfd < 0) {
		fprintf(stderr, "Unable to open directory containing tup config\n");
		return -1;
	}
	fd = openat(dfd, file, O_RDONLY);
	if(fd < 0) {
		if(errno != ENOENT) {
			perror(file);
			return -1;
		}
		/* No tup.config == empty file_tree */
		rc = 0;
	} else {
		/* TODO: Get this straight into atvardb instead? The trick will
		 * be mapping the atvardb var_entries to tup_entrys, since it
		 * will map the new variables (from the file) to the old
		 * tup_entrys (in the database), and will need to updated wrt
		 * ghost nodes and such.
		 */
		rc = get_file_var_tree(&file_tree, fd);
		if(close(fd) < 0) {
			perror("close(fd)");
			return -1;
		}
	}
	if(close(dfd) < 0) {
		perror("close(dfd)");
		return -1;
	}
	if(rc < 0)
		return -1;

	if(vardb_compare(&db_tree, &file_tree, remove_var, add_var,
			 compare_vars, vardt) < 0)
		return -1;

	if(save_vardict_file(&file_tree) < 0)
		return -1;

	vardb_close(&file_tree);
	vardb_close(&db_tree);

	return 0;
}

static struct var_entry *envdb_set(const char *var, int varlen, const char *newenv,
				   int envlen, struct tup_entry *tent, int write_db)
{
	struct var_entry *ve;
	char fullenv[varlen + 1 + envlen + 1];
	char *dbvalue = NULL;

	if(newenv) {
		memcpy(fullenv, var, varlen);
		fullenv[varlen] = '=';
		memcpy(fullenv + varlen + 1, newenv, envlen);
		fullenv[varlen + 1 + envlen] = 0;

		ve = vardb_set2(&envdb, var, varlen, fullenv, tent);
		dbvalue = fullenv;
	} else {
		ve = vardb_set2(&envdb, var, varlen, NULL, tent);
	}
	if(ve && write_db) {
		if(tup_db_set_var(tent->tnode.tupid, dbvalue) < 0)
			return NULL;
	}
	return ve;
}

static int env_cb(void *arg, tupid_t tupid, const char *var, const char *stored_value, int type)
{
	const char *env;
	struct tup_entry *tent;
	int envlen = 0;
	int match = 0;
	struct var_entry *ve;
	int varlen;
	int environ_check = *(int*)arg;
	if(type) {}

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;

	varlen = strlen(var);

	if(environ_check) {
		env = getenv(var);

		if(env) {
			envlen = strlen(env);
			if(stored_value) {
				/* Here we are checking if the stored value of the
				 * environment variable matches the value from getenv().
				 * We store it as "FOO=bar", so we check that the length
				 * of the variable + 1 (for =) + length of the getenv()
				 * matches the length of the stored value. If that
				 * matches, then make sure we match each part.
				 */
				if((signed)strlen(stored_value) == varlen + 1 + envlen &&
				   memcmp(stored_value, var, varlen) == 0 &&
				   stored_value[varlen] == '=' &&
				   memcmp(&stored_value[varlen+1], env, envlen) == 0) {
					match = 1;
				}
			}
		} else {
			/* Both NULL matches */
			if(!stored_value) {
				match = 1;
			}
		}
	} else {
		/* If we aren't checking the environment, just force the match so we don't rebuild
		 * or save the new value. We also want to set our cached value in envdb as
		 * the one that was stored.
		 */
		if(stored_value) {
			env = stored_value + varlen + 1;
			envlen = strlen(env);
		} else {
			env = NULL;
		}
		match = 1;
	}

	if(!match) {
		printf("Environment variable changed: %s\n", var);
		/* Skip past the 'FOO=' part of the stored value if we have an old value to print */
		printf(" - Old: '%s'\n", stored_value ? stored_value + varlen + 1: NULL);
		printf(" - New: '%s'\n", env);
		if(tup_db_add_create_list(tent->tnode.tupid) < 0)
			return -1;
		if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
			return -1;
	}
	ve = envdb_set(var, varlen, env, envlen, tent, !match);
	if(!ve)
		return -1;
	return 0;
}

int tup_db_check_env(int environ_check)
{
	if(tup_db_var_foreach(env_dt(), env_cb, &environ_check) < 0)
		return -1;
	return 0;
}

int tup_db_findenv(const char *var, struct tup_entry **tent)
{
	struct var_entry *ve;
	int varlen = strlen(var);

	ve = vardb_get(&envdb, var, varlen);
	if(!ve) {
		struct tup_entry *newtent;
		const char *newenv;
		int newenvlen = 0;

		newtent = tup_db_node_insert(env_dt(), var, varlen, TUP_NODE_VAR, -1);
		if(!newtent)
			return -1;
		newenv = getenv(var);
		if(newenv)
			newenvlen = strlen(newenv);
		ve = envdb_set(var, varlen, newenv, newenvlen, newtent, 1);
		if(!ve)
			return -1;
	}
	*tent = ve->tent;
	return 0;
}

int tup_db_get_environ(struct tupid_entries *root,
		       struct tupid_entries *normal_root, struct tup_env *te)
{
	struct var_entry *ve;
	struct tupid_tree *tt;
	struct tup_entry *tent;
	char *cur;

	te->block_size = 1;
	te->num_entries = 0;
	RB_FOREACH(tt, tupid_entries, root) {
		tent = tup_entry_find(tt->tupid);
		/* If we don't find the tent, that means it can't be an
		 * environment variable, since all environment variables are
		 * added during the env_cb or during export from the parser.
		 */
		if(tent && tent->dt == env_dt()) {
			ve = vardb_get(&envdb, tent->name.s, tent->name.len);
			if(!ve) {
				fprintf(stderr, "tup internal error: Expected environment variable '%s' to be in envdb.\n", tent->name.s);
				return -1;
			}
			if(ve->value) {
				te->block_size += strlen(ve->value) + 1;
				te->num_entries++;
			}

			/* Remove the environment variable from the normal
			 * tree to make sure that it doesn't get culled after
			 * the command completes. We do this here because we
			 * know we need all environment variables, and they
			 * won't get a corresponding read request during
			 * execution.
			 */
			if(normal_root)
				tupid_tree_remove(normal_root, tt->tupid);
		}
	}
	te->envblock = malloc(te->block_size);
	if(!te->envblock) {
		perror("malloc");
		return -1;
	}
	cur = te->envblock;
	RB_FOREACH(tt, tupid_entries, root) {
		tent = tup_entry_find(tt->tupid);
		if(tent && tent->dt == env_dt()) {
			ve = vardb_get(&envdb, tent->name.s, tent->name.len);
			if(!ve) {
				fprintf(stderr, "tup internal error: Expected environment variable '%s' to be in envdb.\n", tent->name.s);
				return -1;
			}
			if(ve->value) {
				memcpy(cur, ve->value, ve->vallen);
				cur[ve->vallen] = 0;
				cur += ve->vallen + 1;
			}
		}
	}
	*cur = 0;
	return 0;
}

tupid_t env_dt(void)
{
	static tupid_t local_env_dt = -1;
	struct tup_entry *envtent;

	if(local_env_dt != -1)
		return local_env_dt;

	if(tup_entry_add(DOT_DT, NULL) < 0)
		return -1;
	if(tup_db_select_tent(DOT_DT, "$", &envtent) < 0) {
		return -1;
	}
	if(!envtent) {
		envtent = tup_db_create_node(DOT_DT, "$", TUP_NODE_DIR);
		if(!envtent) {
			fprintf(stderr, "tup error: Unable to create virtual '$' directory for environment variables.\n");
			return -1;
		}
	}
	local_env_dt = envtent->tnode.tupid;
	return local_env_dt;
}

tupid_t slash_dt(void)
{
	struct tup_entry *slashtent;

	if(local_slash_dt != -1)
		return local_slash_dt;

	if(tup_entry_add(DOT_DT, NULL) < 0)
		return -1;
	if(tup_db_select_tent(DOT_DT, "/", &slashtent) < 0) {
		return -1;
	}
	if(!slashtent) {
		slashtent = tup_db_create_node(DOT_DT, "/", TUP_NODE_DIR);
		if(!slashtent) {
			fprintf(stderr, "tup error: Unable to create virtual '/' directory for paths outside the tup hierarchy.\n");
			return -1;
		}
	}
	local_slash_dt = slashtent->tnode.tupid;
	return local_slash_dt;
}

int tup_db_scan_begin(struct tupid_entries *root)
{
	if(tup_db_begin() < 0)
		return -1;
	if(files_to_tree(root) < 0)
		return -1;
	tupid_tree_remove(root, env_dt());
	return 0;
}

int tup_db_scan_end(struct tupid_entries *root)
{
	struct tupid_tree *tt;

	while((tt = RB_ROOT(root)) != NULL) {
		struct tup_entry *tent;

		/* It is possible that the node has already been removed. For
		 * example, we may have previously called tup_file_missing on
		 * the directory that owns a file before calling it on the
		 * file. In this case, the tent will no longer exist.
		 */
		tent = tup_entry_find(tt->tupid);
		if(tent) {
			if(tup_file_missing(tent) < 0)
				return -1;
		}
		tupid_tree_rm(root, tt);
		free(tt);
	}

	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int files_to_tree(struct tupid_entries *root)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_FILES_TO_TREE];
	static char s[] = "select id, dir, type, mtime, srcid, name from node where type=? or type=? or type=?";

	transaction_check("%s [37m[%i, %i, %i][0m", s, TUP_NODE_FILE, TUP_NODE_DIR, TUP_NODE_GENERATED);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, TUP_NODE_FILE) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	/* Some things may add entrys before we get here (eg: env_dt()), but
	 * the entry table needs to be clear before we use tup_entry_add_all()
	 */
	tup_entry_clear();

	while(1) {
		tupid_t tupid;
		tupid_t dt;
		int type;
		time_t mtime;
		const char *name;
		tupid_t srcid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			break;
		}

		tupid = sqlite3_column_int64(*stmt, 0);
		dt = sqlite3_column_int64(*stmt, 1);
		type = sqlite3_column_int(*stmt, 2);
		mtime = sqlite3_column_int64(*stmt, 3);
		srcid = sqlite3_column_int64(*stmt, 4);
		name = (const char*)sqlite3_column_text(*stmt, 5);

		if(tup_entry_add_all(tupid, dt, type, mtime, srcid, name, root) < 0)
			break;
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(tup_entry_resolve_dirs() < 0)
		return -1;

	return rc;
}

static int get_output_tree(tupid_t cmdid, struct tupid_entries *output_root)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_OUTPUT_TREE];
	static char s[] = "select to_id from link where from_id=?";

	transaction_check("%s [37m[%lli][0m", s, cmdid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		tupid_t tupid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			break;
		}

		tupid = sqlite3_column_int64(*stmt, 0);
		rc = tupid_tree_add(output_root, tupid);

		if(rc < 0) {
			fprintf(stderr, "tup error: get_output_tree() unable to insert tupid %lli into tree - duplicate output link in the database for command %lli?\n", tupid, cmdid);
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_get_links(tupid_t cmdid, struct tupid_entries *sticky_root,
		     struct tupid_entries *normal_root)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_LINKS];
	static char s[] = "select from_id, style from link where to_id=?";

	transaction_check("%s [37m[%lli][0m", s, cmdid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, cmdid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		int style;
		tupid_t tupid;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			break;
		}

		tupid = sqlite3_column_int64(*stmt, 0);
		style = sqlite3_column_int(*stmt, 1);
		if(style & TUP_LINK_STICKY) {
			rc = tupid_tree_add(sticky_root, tupid);
		}
		if(style & TUP_LINK_NORMAL) {
			rc = tupid_tree_add(normal_root, tupid);
		}

		if(rc < 0) {
			fprintf(stderr, "tup error: tup_db_get_links() unable to insert tupid %lli into tree - duplicate input link in the database for command %lli?\n", tupid, cmdid);
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int compare_list_tree(struct tup_entry_head *a, struct tupid_entries *b,
			     void *data,
			     int (*extra_a)(tupid_t tupid, void *data),
			     int (*extra_b)(tupid_t tupid, void *data))
{
	struct tup_entry *tent;
	struct tupid_tree *ttb;

	LIST_FOREACH(tent, a, list) {
		ttb = tupid_tree_search(b, tent->tnode.tupid);
		if(!ttb) {
			if(extra_a && extra_a(tent->tnode.tupid, data) < 0)
				return -1;
		} else {
			tupid_tree_rm(b, ttb);
			free(ttb);
		}
	}

	RB_FOREACH(ttb, tupid_entries, b) {
		if(extra_b && extra_b(ttb->tupid, data) < 0)
			return -1;
	}

	return 0;
}

static int compare_trees(struct tupid_entries *a, struct tupid_entries *b,
			 void *data,
			 int (*extra_a)(tupid_t tupid, void *data),
			 int (*extra_b)(tupid_t tupid, void *data))
{
	struct tupid_tree *tta;
	struct tupid_tree *ttb;

	tta = RB_MIN(tupid_entries, a);
	ttb = RB_MIN(tupid_entries, b);

	while(tta || ttb) {
		if(!tta) {
			if(extra_b && extra_b(ttb->tupid, data) < 0)
				return -1;
			ttb = RB_NEXT(tupid_entries, b, ttb);
		} else if(!ttb) {
			if(extra_a && extra_a(tta->tupid, data) < 0)
				return -1;
			tta = RB_NEXT(tupid_entries, a, tta);
		} else {
			if(tta->tupid == ttb->tupid) {
				/* Would call same() here if necessary */
				tta = RB_NEXT(tupid_entries, a, tta);
				ttb = RB_NEXT(tupid_entries, b, ttb);
			} else if(tta->tupid < ttb->tupid) {
				if(extra_a && extra_a(tta->tupid, data) < 0)
					return -1;
				tta = RB_NEXT(tupid_entries, a, tta);
			} else {
				if(extra_b && extra_b(ttb->tupid, data) < 0)
					return -1;
				ttb = RB_NEXT(tupid_entries, b, ttb);
			}
		}
	}
	return 0;
}

struct actual_output_data {
	FILE *f;
	tupid_t cmdid;
	int output_error;
};

static int extra_output(tupid_t tupid, void *data)
{
	struct tup_entry *tent;
	struct actual_output_data *aod = data;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;

	if(!(aod->output_error & 1)) {
		aod->output_error |= 1;
		fprintf(aod->f, "tup error: Unspecified output files - A command is writing to files that you didn't specify in the Tupfile. You should add them so tup knows what to expect.\n");
	}

#ifdef _WIN32
	/* TODO: This can be removed once win32 supports tmp files */
	tup_db_modify_cmds_by_output(tent->tnode.tupid, NULL);
	fprintf(aod->f, "[35m -- Delete: %s at dir %lli[0m\n",
		tent->name.s, tent->dt);
	delete_file(tent->dt, tent->name.s);
#else
	fprintf(aod->f, " -- Unspecified output: %s at dir %lli\n", tent->name.s, tent->dt);
#endif
	/* Return success here so we can display all errant outputs.  Actual
	 * check is in tup_db_check_actual_outputs().
	 */
	return 0;
}

static int missing_output(tupid_t tupid, void *data)
{
	struct tup_entry *tent;
	struct actual_output_data *aod = data;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;

	fprintf(aod->f, "tup error: Expected to write to file '%s' from cmd %lli but didn't\n", tent->name.s, aod->cmdid);

	if(!(aod->output_error & 2)) {
		aod->output_error |= 2;
		/* Return success here so we can display all errant outputs.
		 * Actual check is in tup_db_check_actual_outputs().
		 */
	}
	return 0;
}

int tup_db_check_actual_outputs(FILE *f, tupid_t cmdid,
				struct tup_entry_head *writehead)
{
	struct tupid_entries output_root = {NULL};
	struct actual_output_data aod = {
		.f = f,
		.cmdid = cmdid,
		.output_error = 0,
	};

	if(get_output_tree(cmdid, &output_root) < 0)
		return -1;
	if(compare_list_tree(writehead, &output_root, &aod,
			     extra_output, missing_output) < 0)
		return -1;
	free_tupid_tree(&output_root);
	if(aod.output_error)
		return -1;
	return 0;
}

struct write_input_data {
	tupid_t cmdid;
	struct tupid_entries *normal_root;
	struct tupid_entries *delete_root;
	struct tupid_entries *env_root;
};

static int add_sticky(tupid_t tupid, void *data)
{
	struct write_input_data *wid = data;
	int rc;

	if(tupid_tree_search(wid->normal_root, tupid) == NULL) {
		/* Not a normal link, insert it */
		int style = TUP_LINK_STICKY;
		if(tupid_tree_search(wid->env_root, tupid) != NULL) {
			/* Environment links have to be normal so we can
			 * build the graph properly when they are modified.
			 */
			style |= TUP_LINK_NORMAL;

			/* Also need to make sure we run the command in case this
			 * is a new environment variable.
			 */
			if(tup_db_add_modify_list(wid->cmdid) < 0)
				return -1;
		}
		rc = link_insert(tupid, wid->cmdid, style);
	} else {
		/* Currently just a normal link, update it */
		rc = link_update(tupid, wid->cmdid, TUP_LINK_NORMAL | TUP_LINK_STICKY);
	}
	return rc;
}

static int rm_sticky(tupid_t tupid, void *data)
{
	struct write_input_data *wid = data;

	if(tupid_tree_search(wid->normal_root, tupid) == NULL) {
		/* Not a normal link, kill it */
		if(link_remove(tupid, wid->cmdid) < 0)
			return -1;
	} else {
		if(tupid_tree_search(wid->delete_root, tupid) == NULL) {
			/* Demote to a normal link */
			if(link_update(tupid, wid->cmdid, TUP_LINK_NORMAL) < 0)
				return -1;
		} else {
			/* The node is in the delete list and we are no longer
			 * claiming it as a dependency. Make sure the normal
			 * link is removed as well to avoid a circular
			 * dependency (t6045).
			 */
			if(link_remove(tupid, wid->cmdid) < 0)
				return -1;
		}
		/* Make sure we re-run the command to check for required
		 * inputs.
		 */
		if(tup_db_add_modify_list(wid->cmdid) < 0)
			return -1;
	}
	return 0;
}

int tup_db_write_inputs(tupid_t cmdid, struct tupid_entries *input_root,
			struct tupid_entries *env_root,
			struct tupid_entries *delete_root)
{
	struct tupid_entries sticky_root = {NULL};
	struct tupid_entries normal_root = {NULL};
	struct write_input_data wid = {
		.cmdid = cmdid,
		.normal_root = &normal_root,
		.delete_root = delete_root,
		.env_root = env_root,
	};

	if(tup_db_get_links(cmdid, &sticky_root, &normal_root) < 0)
		return -1;
	if(compare_trees(input_root, &sticky_root, &wid,
			 add_sticky, rm_sticky) < 0)
		return -1;
	free_tupid_tree(&sticky_root);
	free_tupid_tree(&normal_root);
	return 0;
}

struct actual_input_data {
	FILE *f;
	tupid_t cmdid;
	struct tupid_entries *sticky_root;
	struct tupid_entries output_root;
	struct tupid_entries missing_input_root;
};

static int new_input(tupid_t tupid, void *data)
{
	struct tup_entry *tent;
	struct actual_input_data *aid = data;

	/* Skip any files that are supposed to be used as outputs */
	if(tupid_tree_search(&aid->output_root, tupid) != NULL)
		return 0;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	if(tent->type == TUP_NODE_GENERATED) {
		if(tupid_tree_add(&aid->missing_input_root, tent->tnode.tupid) < 0)
			return -1;
		return 0;
	}
	return 0;
}

static int new_normal_link(tupid_t tupid, void *data)
{
	struct actual_input_data *aid = data;
	int rc;

	/* Skip any files that are supposed to be used as outputs */
	if(tupid_tree_search(&aid->output_root, tupid) != NULL)
		return 0;
	/* t6057 - Skip any files that were reported as errors in new_input() */
	if(tupid_tree_search(&aid->missing_input_root, tupid) != NULL)
		return 0;

	if(tupid_tree_search(aid->sticky_root, tupid) == NULL) {
		struct tup_entry *tent;

		if(tup_entry_add(tupid, &tent) < 0)
			return -1;
		if(tent->type == TUP_NODE_CMD) {
			fprintf(aid->f, "tup error: Attempted to read from a file with the same name as an existing command string. Existing command is: ");
			print_tup_entry(aid->f, tent);
			fprintf(aid->f, "\n");
			return -1;
		}
		/* Not a sticky link, insert it */
		rc = link_insert(tupid, aid->cmdid, TUP_LINK_NORMAL);
	} else {
		/* Currently just a sticky link, update it */
		rc = link_update(tupid, aid->cmdid, TUP_LINK_NORMAL | TUP_LINK_STICKY);
	}
	return rc;
}

static int del_normal_link(tupid_t tupid, void *data)
{
	struct actual_input_data *aid = data;

	if(tupid_tree_search(aid->sticky_root, tupid) == NULL) {
		/* Not a sticky link, kill it. Also check if it was a ghost
		 * (t5054).
		 */
		if(link_remove(tupid, aid->cmdid) < 0)
			return -1;
		if(add_ghost(tupid) < 0)
			return -1;
	} else {
		/* Demote to a sticky link */
		if(link_update(tupid, aid->cmdid, TUP_LINK_STICKY) < 0)
			return -1;
	}
	return 0;
}

static int check_generated_inputs(FILE *f, struct tupid_entries *missing_input_root,
				  struct tupid_entries *valid_input_root)
{
	int found_error = 0;
	struct tupid_tree *tt;
	struct tupid_tree *tmp;

	/* First, repeatedly go through the list of missing inputs (ie:
	 * generated files that we read from, but didn't specify in the Tupfile)
	 * to see if we can reach them from another generated file that we
	 * *did* specify in the Tupfile.
	 */
	RB_FOREACH_SAFE(tt, tupid_entries, missing_input_root, tmp) {
		struct tup_entry *tent;
		int connected;
		if(tup_entry_add(tt->tupid, &tent) < 0)
			return -1;
		if(nodes_are_connected(tent, valid_input_root, &connected) < 0)
			return -1;

		if(connected) {
			tupid_tree_rm(missing_input_root, tt);
			free(tt);
		}
	}

	/* Anything we couldn't connect is an error. */
	RB_FOREACH(tt, tupid_entries, missing_input_root) {
		if(!found_error) {
			fprintf(f, "tup error: Missing input dependency - a file was read from, and was not specified as an input link for the command. This is an issue because the file was created from another command, and without the input link the commands may execute out of order. You should add this file as an input, since it is possible this could randomly break in the future.\n");
			found_error = 1;
		}
		tup_db_print(f, tt->tupid);
	}
	if(found_error)
		return -1;
	return 0;
}

int tup_db_check_actual_inputs(FILE *f, tupid_t cmdid,
			       struct tup_entry_head *readhead,
			       struct tupid_entries *sticky_root,
			       struct tupid_entries *normal_root)
{
	struct tupid_entries sticky_copy = {NULL};
	struct actual_input_data aid = {
		.f = f,
		.cmdid = cmdid,
		.sticky_root = sticky_root,
		.output_root = {NULL},
		.missing_input_root = {NULL},
	};
	int rc;

	if(get_output_tree(cmdid, &aid.output_root) < 0)
		return -1;
	if(tupid_tree_copy(&sticky_copy, aid.sticky_root) < 0)
		return -1;
	/* First check if we are missing any links that should be sticky. We
	 * don't care about any links that are marked sticky but aren't used.
	 */
	if(compare_list_tree(readhead, &sticky_copy, &aid,
			     new_input, NULL) < 0)
		return -1;

	rc = check_generated_inputs(f, &aid.missing_input_root, aid.sticky_root);

	if(compare_list_tree(readhead, normal_root, &aid,
			     new_normal_link, del_normal_link) < 0)
		return -1;
	free_tupid_tree(&sticky_copy);
	free_tupid_tree(&aid.output_root);
	free_tupid_tree(&aid.missing_input_root);
	return rc;
}

struct parse_output_data {
	tupid_t cmdid;
	int outputs_differ;
};

static int add_output(tupid_t tupid, void *data)
{
	struct parse_output_data *pod = data;

	pod->outputs_differ = 1;
	if(link_insert(pod->cmdid, tupid, TUP_LINK_NORMAL) < 0)
		return -1;
	return 0;
}

static int rm_output(tupid_t tupid, void *data)
{
	struct parse_output_data *pod = data;

	pod->outputs_differ = 1;
	if(link_remove(pod->cmdid, tupid) < 0)
		return -1;
	return 0;
}

int tup_db_write_outputs(tupid_t cmdid, struct tupid_entries *root)
{
	struct tupid_entries output_root = {NULL};
	struct parse_output_data pod = {
		.cmdid = cmdid,
		.outputs_differ = 0,
	};

	if(get_output_tree(cmdid, &output_root) < 0)
		return -1;
	if(compare_trees(&output_root, root, &pod, rm_output, add_output) < 0)
		return -1;
	if(pod.outputs_differ == 1) {
		if(tup_db_add_modify_list(cmdid) < 0)
			return -1;
	}
	free_tupid_tree(&output_root);
	return 0;
}

struct write_dir_input_data {
	tupid_t dt;
};

static int add_dir_link(tupid_t tupid, void *data)
{
	struct write_dir_input_data *wdid = data;
	struct tup_entry *tent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	if(tent->type == TUP_NODE_GENERATED) {
		fprintf(stderr, "tup error: Unable to read from generated file '");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "'. Your build configuration must be comprised of files you wrote yourself.\n");
		return -1;
	}
	if(link_insert(tupid, wdid->dt, TUP_LINK_NORMAL) < 0)
		return -1;
	return 0;
}

static int rm_dir_link(tupid_t tupid, void *data)
{
	struct write_dir_input_data *wdid = data;

	if(add_ghost(tupid) < 0)
		return -1;
	if(link_remove(tupid, wdid->dt) < 0)
		return -1;
	return 0;
}

int tup_db_write_dir_inputs(tupid_t dt, struct tupid_entries *root)
{
	struct tupid_entries sticky_root = {NULL};
	struct tupid_entries normal_root = {NULL};
	struct write_dir_input_data wdid = {
		.dt = dt,
	};

	if(tup_db_get_links(dt, &sticky_root, &normal_root) < 0)
		return -1;
	if(!RB_EMPTY(&sticky_root)) {
		/* All links to directories should be TUP_LINK_NORMAL */
		fprintf(stderr, "tup internal error: sticky link found to dir %lli\n", dt);
		return -1;
	}
	if(compare_trees(root, &normal_root, &wdid,
			 add_dir_link, rm_dir_link) < 0)
		return -1;
	free_tupid_tree(&sticky_root);
	free_tupid_tree(&normal_root);
	return 0;
}

struct tup_entry *tup_db_node_insert(tupid_t dt, const char *name, int len,
				     int type, time_t mtime)
{
	struct tup_entry *tent;
	if(tup_db_node_insert_tent(dt, name, len, type, mtime, -1, &tent) < 0)
		return NULL;
	return tent;
}

int tup_db_node_insert_tent(tupid_t dt, const char *name, int len, int type,
			    time_t mtime, tupid_t srcid, struct tup_entry **entry)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_NODE_INSERT];
	static char s[] = "insert into node(dir, type, name, mtime, srcid) values(?, ?, ?, ?, ?)";
	tupid_t tupid;

	transaction_check("%s [37m[%lli, %i, '%.*s', %li, %lli][0m", s, dt, type, len, name, mtime, srcid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 3, name, len, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 4, mtime) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 5, srcid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	tupid = sqlite3_last_insert_rowid(tup_db);
	if(type == TUP_NODE_CMD) {
		/* New commands go in the modify list so they are executed at
		 * least once.
		 */
		if(tup_db_add_modify_list(tupid) < 0)
			return -1;
	}

	if(tup_entry_add_to_dir(dt, tupid, name, len, type, mtime, srcid, entry) < 0)
		return -1;

	return 0;
}

static int node_select(tupid_t dt, const char *name, int len,
		       struct tup_entry **entry)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODE_SELECT];
	tupid_t tupid;
	int type;
	int mtime;
	tupid_t srcid;
	static char s[] = "select id, type, mtime, srcid from node where dir=? and name=?" SQL_NAME_COLLATION;

	*entry = NULL;

	if(tup_entry_find_name_in_dir(dt, name, len, entry) < 0)
		return -1;
	if(*entry)
		return 0;

	transaction_check("%s [37m[%lli, '%.*s'][0m", s, dt, len, name);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 2, name, len, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}

	rc = 0;
	tupid = sqlite3_column_int64(*stmt, 0);
	type = sqlite3_column_int(*stmt, 1);
	mtime = sqlite3_column_int(*stmt, 2);
	srcid = sqlite3_column_int64(*stmt, 3);

	if(tup_entry_add_to_dir(dt, tupid, name, len, type, mtime, srcid, entry) < 0) {
		rc = -1;
		goto out_reset;
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
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
	if(a <= 0 || b <= 0) {
		fprintf(stderr, "tup error: Attmept to insert invalid link: %lli -> %lli\n", a, b);
		return -1;
	}

	transaction_check("%s [37m[%lli, %lli, %i][0m", s, a, b, style);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 3, style) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static int link_update(tupid_t a, tupid_t b, int style)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_LINK_UPDATE];
	static char s[] = "update link set style=? where from_id=? and to_id=?";

	transaction_check("%s [37m[%i, %lli, %lli][0m", s, style, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int(*stmt, 1, style) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 3, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static int link_remove(tupid_t a, tupid_t b)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_LINK_REMOVE];
	static char s[] = "delete from link where from_id=? and to_id=?";

	transaction_check("%s [37m[%lli, %lli][0m", s, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static int node_has_ghosts(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODE_HAS_GHOSTS];
	static char s[] = "select id from node where dir=?";

	/* This is used to determine if we need to make a real node into a
	 * ghost node. We only need to do that if some other node references it
	 * via dir. We don't care about links because nothing will have a link
	 * to a ghost.
	 */
	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	dbrc = sqlite3_step(*stmt);
	if(dbrc == SQLITE_DONE) {
		rc = 0;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		rc = -1;
		goto out_reset;
	}
	rc = 1;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int add_ghost(tupid_t tupid)
{
	struct tup_entry *tent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;

	tup_entry_add_ghost_list(tent, &ghost_list);

	return 0;
}

static int add_ghost_links(tupid_t tupid)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_ADD_GHOST_LINKS];
	static char s[] = "select from_id from link where to_id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	do {
		tupid_t link_tupid;
		struct tup_entry *tent;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			break;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			rc = -1;
			goto out_reset;
		}

		link_tupid = sqlite3_column_int64(*stmt, 0);
		if(tup_entry_add(link_tupid, &tent) < 0) {
			rc = -1;
			goto out_reset;
		}

		tup_entry_add_ghost_list(tent, &ghost_list);
	} while(1);

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int reclaim_ghosts(void)
{
	/* All the nodes in ghost_list already are of type TUP_NODE_GHOST. Just
	 * make sure they are no longer needed before deleting them by checking:
	 *  - no other node references it in 'dir'
	 *  - no other node is pointed to by it
	 *  - we are not a ghost 'tup.config' file used for holding @-variables.
	 *
	 *  (see ghost_reclaimable())
	 *
	 * If all those cases check out then the ghost can be removed. If the
	 * ghost is removed then its parent directory is re-added to the list
	 * if it is a ghost dir in order to handle things like a ghost dir
	 * having a ghost subdir - the subdir would be removed in one pass,
	 * then the other dir in the next pass.
	 */
	struct tup_entry_head tmp_list;

	LIST_INIT(&tmp_list);

	while(!LIST_EMPTY(&ghost_list)) {
		struct tup_entry *tent;
		int rc;

		tent = LIST_FIRST(&ghost_list);
		if(tent->type != TUP_NODE_GHOST) {
			fprintf(stderr, "tup internal error: tup entry %lli in the ghost_list shouldn't be type %i\n", tent->tnode.tupid, tent->type);
			return -1;
		}
		if(tup_entry_del_ghost_list(tent) < 0)
			return -1;

		rc = ghost_reclaimable(tent->tnode.tupid);
		if(rc < 0)
			return -1;
		if(rc == 1 && strcmp(tent->name.s, TUP_CONFIG) != 0) {
			if(sql_debug || reclaim_ghost_debug) {
				fprintf(stderr, "Ghost removed: %lli\n", tent->tnode.tupid);
			}

			/* Re-check the parent again later */
			tup_entry_add_ghost_list(tent->parent, &tmp_list);

			if(delete_node(tent->tnode.tupid) < 0)
				return -1;
		}

		if(LIST_EMPTY(&ghost_list)) {
			LIST_SWAP(&ghost_list, &tmp_list, tup_entry, ghost_list);
		}
	}

	return 0;
}

static int ghost_reclaimable(tupid_t tupid)
{
	int rc1, rc2;

	rc1 = ghost_reclaimable1(tupid);
	rc2 = ghost_reclaimable2(tupid);

	/* If either sub-query fails, we fail */
	if(rc1 < 0 || rc2 < 0)
		return -1;
	/* If both checks say it is reclaimable, then it is reclaimable */
	if(rc1 == 1 && rc2 == 1)
		return 1;
	/* Otherwise, it is not reclaimable */
	return 0;
}

static int ghost_reclaimable1(tupid_t tupid)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GHOST_RECLAIMABLE1];
	static char s[] = "select exists(select 1 from node where dir=?)";

	transaction_check("%s [37m[%lli, %lli][0m", s, tupid, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);

	if(rc == SQLITE_DONE) {
		fprintf(stderr, "tup error: Expected ghost_reclaimable1() to get an SQLite row returned.\n");
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}
	dbrc = sqlite3_column_int(*stmt, 0);
	/* If the exists clause returns 0, then we are reclaimable, otherwise we aren't */
	if(dbrc == 0) {
		rc = 1;
	} else if(dbrc == 1) {
		rc = 0;
	} else {
		fprintf(stderr, "tup error: Expected ghost_reclaimable1() to get a 0 or 1 from SQLite\n");
		goto out_reset;
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int ghost_reclaimable2(tupid_t tupid)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GHOST_RECLAIMABLE2];
	static char s[] = "select exists(select 1 from link where from_id=?)";

	transaction_check("%s [37m[%lli, %lli][0m", s, tupid, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);

	if(rc == SQLITE_DONE) {
		fprintf(stderr, "tup error: Expected ghost_reclaimable2() to get an SQLite row returned.\n");
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}
	dbrc = sqlite3_column_int(*stmt, 0);
	/* If the exists clause returns 0, then we are reclaimable, otherwise we aren't */
	if(dbrc == 0) {
		rc = 1;
	} else if(dbrc == 1) {
		rc = 0;
	} else {
		fprintf(stderr, "tup error: Expected ghost_reclaimable2() to get a 0 or 1 from SQLite\n");
		goto out_reset;
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int get_db_var_tree(tupid_t dt, struct vardb *vdb)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_DB_VAR_TREE];
	static char s[] = "select node.id, name, value, type from node, var where dir=? and node.id=var.id";

	transaction_check("%s [37m[%i][0m", s, dt);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dt) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	/* t7042 - make sure we have the dt entry */
	if(tup_entry_add(dt, NULL) < 0)
		return -1;

	do {
		tupid_t tupid;
		const char *var;
		const char *value;
		int type;
		struct tup_entry *tent;

		dbrc = sqlite3_step(*stmt);
		if(dbrc == SQLITE_DONE) {
			rc = 0;
			goto out_reset;
		}
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			goto out_reset;
		}

		tupid = sqlite3_column_int64(*stmt, 0);
		var = (const char*)sqlite3_column_text(*stmt, 1);
		value = (const char*)sqlite3_column_text(*stmt, 2);
		type = sqlite3_column_int(*stmt, 3);
		/* Only add the entry if we don't have it already. It is
		 * possible that variables have been added if a file was
		 * removed, causing incoming links to be added to the
		 * by add_ghost_links.
		 */
		tent = tup_entry_find(tupid);
		if(!tent)
			if(tup_entry_add_to_dir(dt, tupid, var, -1, type, -1, -1, &tent) <0)
				goto out_reset;
		if(vardb_set(vdb, var, value, tent) < 0)
			goto out_reset;
	} while(1);

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

static int get_file_var_tree(struct vardb *vdb, int fd)
{
	struct buf b;
	char *p;

	if(fslurp(fd, &b) < 0)
		return -1;

	p = b.s;
	while(p < b.s + b.len) {
		char *nl;

		nl = strchr(p, '\n');
		if(!nl) {
			fprintf(stderr, "tup error: No newline found in tup config file\n");
			return -1;
		}

		while (nl > p && isspace(nl[-1]))
			nl--;

		*nl = 0;

		if(nl == p)
			goto skip;

		if(p[0] == '#') {
			if(strncmp(p, "# CONFIG_", 9) == 0) {
				char *space;
				space = strchr(p+9, ' ');
				if(!space) {
					fprintf(stderr, "tup error: No space found in tup config.\nLine was: '%s'\n", p);
					return -1;
				}
				*space = 0;
				if(vardb_set(vdb, p+9, "n", NULL) < 0)
					return -1;
			}
		} else  {
			char *eq;
			char *value;
			if(strncmp(p, "CONFIG_", 7) != 0) {
				fprintf(stderr, "tup error: Non-comment line in tup config doesn't begin with \"CONFIG_\"\nLine was: '%s'\n", p);
				return -1;
			}
			eq = strchr(p, '=');
			if(!eq) {
				fprintf(stderr, "tup error: No equals sign found in tup config.\nLine was: '%s'\n", p);
				return -1;
			}
			if(eq[1] == '"') {
				char *quote;
				value = eq+2;
				quote = strchr(value, '"');
				if(!quote) {
					fprintf(stderr, "tup error: No end quote found in tup config.\nLine was: '%s'\n", p);
					return -1;
				}
				*quote = 0;
			} else {
				value = eq+1;
			}
			*eq = 0;
			if(vardb_set(vdb, p+7, value, NULL) < 0)
				return -1;
		}

skip:
		p = nl + 1;
	}

	free(b.s);
	return 0;
}

static int var_flag_dirs(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_VAR_FLAG_DIRS];
	static char s[] = "insert or ignore into create_list select to_id from link, node where from_id=? and to_id=node.id and node.type=?";

	transaction_check("%s [37m[%lli, %i][0m", s, tupid, TUP_NODE_DIR);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static int delete_var_entry(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_DELETE_VAR_ENTRY];
	static char s[] = "delete from var where id=?";

	transaction_check("%s [37m[%lli][0m", s, tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return 0;
}

static int no_sync(void)
{
	char *errmsg;
	char sql[] = "PRAGMA synchronous=OFF";

	/* This can't use a transaction_check() because SQLite doesn't allow
	 * changing the synchronicity inside a transaction.
	 */
	if(sql_debug) fprintf(stderr, "%s\n", sql);
	if(sqlite3_exec(tup_db, sql, NULL, NULL, &errmsg) != 0) {
		fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
			errmsg, sql);
		return -1;
	}
	return 0;
}
