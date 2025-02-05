/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2024  Mike Shal <marfey@gmail.com>
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
#include "tupid_list.h"
#include "file.h"
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
#include "variant.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include "sqlite3/sqlite3.h"

#define DB_VERSION 19
#define PARSER_VERSION 16

enum {
	DB_BEGIN,
	DB_COMMIT,
	DB_ROLLBACK,
	DB_FILL_TUP_ENTRY,
	DB_SELECT_NODE_BY_FLAGS_1,
	DB_SELECT_NODE_BY_FLAGS_2,
	DB_SELECT_NODE_BY_FLAGS_3,
	DB_SELECT_NODE_BY_FLAGS_4,
	DB_SELECT_NODE_BY_FLAGS_5,
	DB_SELECT_NODE_DIR,
	DB_SELECT_NODE_DIR_GLOB,
	DB_DELETE_NODE,
	DB_CHDIR,
	DB_SET_NAME,
	DB_SET_DISPLAY,
	DB_SET_FLAGS,
	DB_SET_TYPE,
	DB_SET_MTIME,
	DB_SET_SRCID,
	DB_PRINT,
	DB_REBUILD_ALL,
	DB_WRITE_GITIGNORE,
	DB_MAYBE_ADD_CONFIG_LIST,
	DB_ADD_CONFIG_LIST,
	DB_MAYBE_ADD_CREATE_LIST,
	DB_ADD_CREATE_LIST,
	DB_ADD_MODIFY_LIST,
	DB_ADD_VARIANT_LIST,
	DB_ADD_TRANSIENT_LIST,
	DB_IN_CREATE_LIST,
	DB_IN_MODIFY_LIST,
	DB_IN_TRANSIENT_LIST,
	DB_UNFLAG_CONFIG,
	DB_UNFLAG_CREATE,
	DB_UNFLAG_MODIFY,
	DB_UNFLAG_VARIANT,
	DB_UNFLAG_TRANSIENT,
	_DB_GET_DIR_ENTRIES,
	_DB_GET_OUTPUT_GROUP,
	DB_LINK_EXISTS1,
	DB_LINK_EXISTS2,
	_DB_DELETE_NORMAL_LINKS,
	_DB_DELETE_NORMAL_INPUTS,
	_DB_DELETE_STICKY_LINKS,
	_DB_FLAG_GROUP_USERS1,
	_DB_FLAG_GROUP_USERS2,
	DB_DIRTYPE,
	DB_SRCID_TO_TREE,
	DB_TYPE_TO_TREE,
	_DB_IS_GENERATED_DIR1,
	_DB_IS_GENERATED_DIR2,
	DB_MODIFY_CMDS_BY_OUTPUT,
	DB_MODIFY_CMDS_BY_INPUT,
	DB_SET_DEPENDENT_DIR_FLAGS,
	DB_SET_SRCID_DIR_FLAGS,
	DB_SET_DEPENDENT_CONFIG_FLAGS,
	_DB_GET_OUTPUTS,
	DB_SELECT_NODE_BY_GROUP_LINK,
	DB_SELECT_NODE_BY_DISTINCT_GROUP_LINK,
	DB_CONFIG_SET_INT,
	DB_CONFIG_GET_INT,
	DB_SET_VAR,
	_DB_GET_VAR_ID,
	DB_FILES_TO_TREE,
	_DB_GET_LINKS1,
	_DB_GET_LINKS2,
	_DB_GET_STICKY_OUTPUTS,
	DB_NODE_INSERT,
	_DB_NODE_SELECT,
	_DB_LINK_INSERT1,
	_DB_LINK_INSERT2,
	_DB_LINK_REMOVE1,
	_DB_LINK_REMOVE2,
	_DB_GROUP_LINK_INSERT,
	_DB_GROUP_LINK_REMOVE,
	_DB_DELETE_GROUP_LINKS,
	_DB_NODE_HAS_GHOSTS,
	_DB_EXCLUSION_RECLAIMABLE,
	_DB_GROUP_RECLAIMABLE1,
	_DB_GROUP_RECLAIMABLE2,
	_DB_GHOST_RECLAIMABLE1,
	_DB_GHOST_RECLAIMABLE2,
	DB_GET_VARDB,
	_DB_VAR_FLAG_DIRS,
	_DB_DELETE_VAR_ENTRY,
	DB_NUM_STATEMENTS
};

struct half_entry {
	LIST_ENTRY(half_entry) list;
	tupid_t tupid;
	tupid_t dt;
	enum TUP_NODE_TYPE type;
};
LIST_HEAD(half_entry_head, half_entry);

struct timespec INVALID_MTIME = {-1, 0};
struct timespec EXTERNAL_DIRECTORY_MTIME = {0, 0};

static sqlite3 *tup_db = NULL;
static sqlite3_stmt *stmts[DB_NUM_STATEMENTS];
static struct tent_entries ghost_root = TENT_ENTRIES_INITIALIZER;
static int tup_db_var_changed = 0;
static int sql_debug = 0;
static int reclaim_ghost_debug = 0;
static struct vardb envdb = { {NULL}, 0};
static int transaction = 0;
static tupid_t local_env_dt = -1;
static tupid_t local_exclusion_dt = -1;
static tupid_t local_slash_dt = -1;

/* Simple counter to invalidate the tent->stickies field. If
 * tent->retrieved_stickies is less than the sticky_count, then we need to
 * reload the stickies from the database. The sticky links can become stale
 * when we delete nodes from the database, since the delete_sticky_links()
 * function doesn't go through all tents to update them.
 */
static int sticky_count = 1;

static int version_check(void);
static int init_virtual_dirs(void);
static struct tup_entry *node_insert(struct tup_entry *dtent, const char *name, int namelen,
				     const char *display, int displaylen, const char *flags, int flagslen,
				     enum TUP_NODE_TYPE type, struct timespec mtime, tupid_t srcid);
static int node_select(struct tup_entry *dtent, const char *name, int len,
		       struct tup_entry **entry);

static int link_insert(tupid_t a, tupid_t b, int style);
static int link_remove(tupid_t a, tupid_t b, int style);
static int group_link_insert(tupid_t a, tupid_t b, tupid_t cmdid);
static int group_link_remove(tupid_t a, tupid_t b, tupid_t cmdid);
static int get_sticky_outputs(tupid_t tupid, struct tent_entries *root);
static int delete_group_links(tupid_t cmdid);
static int get_normal_inputs(tupid_t cmdid, struct tent_entries *root, int ghost_check);
static int node_has_ghosts(tupid_t tupid);
static int load_existing_nodes(void);
static int add_ghost_checks(tupid_t tupid);
static int add_group_and_exclusion_checks(tupid_t tupid);
static int reclaim_ghosts(void);
static int ghost_reclaimable(struct tup_entry *tent);
static int exclusion_reclaimable(tupid_t tupid);
static int group_reclaimable1(tupid_t tupid);
static int group_reclaimable2(tupid_t tupid);
static int ghost_reclaimable1(tupid_t tupid);
static int ghost_reclaimable2(tupid_t tupid);
static int get_file_var_tree(struct vardb *vdb, int fd);
static int var_flag_dirs(tupid_t tupid);
static int delete_var_entry(tupid_t tupid);
static int no_sync(void);
static int delete_node(tupid_t tupid);
static int db_print(FILE *stream, tupid_t tupid);
static int get_dir_entries(tupid_t dt, struct half_entry_head *head);

static char transaction_buf[1024];
static int transaction_started = 0;
static struct timespan transaction_ts;

static void transaction_check(const char *format, ...)
{
	if(transaction_started) {
		fprintf(stderr, "tup internal error: Still in a previous database transaction.\n");
		exit(1);
	}
	transaction_started = 1;
	if(sql_debug || !transaction) {
		va_list ap;
		va_start(ap, format);

		timespan_start(&transaction_ts);
		vsnprintf(transaction_buf, sizeof(transaction_buf), format, ap);
		transaction_buf[sizeof(transaction_buf)-1] = 0;
		va_end(ap);
		if(!transaction) {
			fprintf(stderr, "tup internal error: Database query must be in a transaction: %s\n", transaction_buf);
			exit(1);
		}
	}
}

static int msqlite3_reset(sqlite3_stmt *stmt)
{
	transaction_started = 0;
	if(sql_debug) {
		timespan_end(&transaction_ts);
		if(strncmp(transaction_buf, "insert", 6) == 0 ||
		   strncmp(transaction_buf, "update", 6) == 0 ||
		   strncmp(transaction_buf, "delete", 6) == 0) {
			fprintf(stderr, "[%fs] {%i} %s\n", timespan_seconds(&transaction_ts), sqlite3_changes(tup_db), transaction_buf);
		} else {
			fprintf(stderr, "[%fs] %s\n", timespan_seconds(&transaction_ts), transaction_buf);
		}
	}
	return sqlite3_reset(stmt);
}

static int db_open(void)
{
	int x;
	int db_sync;

	if(tup_db)
		return 0;
#ifdef USE_DOTLOCK
	if(sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, "unix-dotfile") != 0) {
#else
	if(sqlite3_open_v2(TUP_DB_FILE, &tup_db, SQLITE_OPEN_READWRITE, NULL) != 0) {
#endif
		fprintf(stderr, "Unable to open database: %s\n",
			sqlite3_errmsg(tup_db));
		return -1;
	}
	for(x=0; x<ARRAY_SIZE(stmts); x++) {
		stmts[x] = NULL;
	}

	db_sync = tup_option_get_flag("db.sync");
	if(db_sync == 0)
		if(no_sync() < 0)
			return -1;
	return 0;
}

int tup_db_open(void)
{
	if(db_open() < 0)
		return -1;
	if(tup_db_begin() < 0)
		return -1;
	if(version_check() < 0)
		return -1;
	if(init_virtual_dirs() < 0)
		return -1;
	if(tup_db_commit() < 0)
		return -1;
	return 0;
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
	tup_db = NULL;
	return 0;
}

int tup_db_create(int db_sync, int memory_db)
{
	int rc;
	int x;
	const char *dbname;
	const char *sql[] = {
		"create table node (id integer primary key not null, dir integer not null, type integer not null, mtime integer not null, mtime_ns integer not null, srcid integer not null, name varchar(4096), display varchar(4096), flags varchar(256), unique(dir, name))",
		"create table normal_link (from_id integer, to_id integer, unique(from_id, to_id))",
		"create table sticky_link (from_id integer, to_id integer, unique(from_id, to_id))",
		"create table group_link (from_id integer, to_id integer, cmdid integer, unique(from_id, to_id, cmdid))",
		"create table var (id integer primary key not null, value varchar(4096))",
		"create table config(lval varchar(256) unique, rval varchar(256))",
		"create table config_list (id integer primary key not null)",
		"create table create_list (id integer primary key not null)",
		"create table modify_list (id integer primary key not null)",
		"create table variant_list (id integer primary key not null)",
		"create table transient_list (id integer primary key not null)",
		"create index normal_index2 on normal_link(to_id)",
		"create index sticky_index2 on sticky_link(to_id)",
		"create index group_index2 on group_link(cmdid)",
		"create index srcid_index on node(srcid)",
		"insert into config values('db_version', 0)",
		"insert into node values(1, 0, 2, -1, 0, -1, '.', NULL, NULL)",
	};

	if(memory_db) {
		dbname = ":memory:";
	} else {
		dbname = TUP_DB_FILE;
	}
#ifdef USE_DOTLOCK
	rc = sqlite3_open_v2(dbname, &tup_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "unix-dotfile");
#else
	rc = sqlite3_open_v2(dbname, &tup_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
#endif
	if(rc == 0) {
		printf(".tup repository initialized: %s\n", dbname);
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
	if(init_virtual_dirs() < 0)
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

	/* Close our current database file, since Windows doesn't let us open
	 * it again for backup.
	 */
	if(tup_db_commit() < 0)
		return -1;
	if(tup_db_close() < 0)
		return -1;

	if(snprintf(backup, sizeof(backup), ".tup/db_backup_%i", version) >=
	   (signed)sizeof(backup)) {
		fprintf(stderr, "tup internal error: db backup buffer mis-sized.\n");
		return -1;
	}
	fd = open(backup, O_CREAT|O_WRONLY|O_TRUNC, 0666);
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
	if(db_open() < 0)
		return -1;
	if(tup_db_begin() < 0)
		return -1;
	return 0;

err_fail:
	fprintf(stderr, "tup error: Unable to backup database.\n");
	close(ifd);
err_open:
	close(fd);
	return -1;
}

int tup_db_get_tup_config_tent(struct tup_entry **tent)
{
	struct tup_entry *vartent = NULL;
	struct tup_entry *root_tent;
	if(tup_entry_add(DOT_DT, &root_tent) < 0)
		return -1;
	if(tup_db_select_tent(root_tent, TUP_CONFIG, &vartent) < 0) {
		fprintf(stderr, "tup internal error: Unable to check for tup.config node in the project root.\n");
		return -1;
	}
	if(!vartent) {
		vartent = tup_db_create_node(root_tent, TUP_CONFIG, TUP_NODE_GHOST);
		if(!vartent) {
			fprintf(stderr, "tup internal error: Unable to create virtual node for tup.config changes in the project root.\n");
			return -1;
		}
	}
	*tent = vartent;
	return 0;
}

#define MAX_UPGRADE 13
struct sql_upgrade {
	const char *message;
	const char *statements[MAX_UPGRADE];
};

static int version_check(void)
{
	int version;
	int x, y;
	char *errmsg;
	struct sql_upgrade upgrades[] = {
		{
			/* Version 1 is built-in. */
			"Unused",
			{
			}
		},
		{
			/* Upgrade to version 2 */
			"The link table has a new column (style) to annotate the origin of the link. This is used to differentiate between links specified in Tupfiles vs. links determined automatically via wrapped command execution, so the links can be removed at appropriate times. Also, a new node type (TUP_NODE_GENERATED==4) has been added. All files created from commands have been updated to this new type. This is used so you can't try to create a command to write to a base source file. All Tupfiles will be re-parsed on the next update in order to generate the new links. If you have any problems, it might be easiest to re-checkout your code and start anew. Admittedly I haven't tested the conversion completely.",
			{
				"alter table link add column style integer default 0",
				"create table create_list (id integer primary key not null)",
				"create table modify_list (id integer primary key not null)",
				"create table delete_list (id integer primary key not null)",
				"create table node_new (id integer primary key not null, dir integer not null, type integer not null, name varchar(4096))",
				"insert or ignore into node_new select id, dir, type, name from node",
				"drop index node_dir_index",
				"drop index node_flags_index",
				"drop table node",
				"alter table node_new rename to node",
				"create index node_dir_index on node(dir, name)",
				"insert or replace into create_list select id from node where type=2 and not id=2",
				"update node set type=4 where id in (select to_id from link) and type=0",
			}
		},
		{
			/* Upgrade to version 3 */
			"The style column in the link table now uses flags instead of multiple records. For example, a link from ID 5 to 7 used to contain 5|7|0 for a normal link and 5|7|1 for a sticky link. Now it is 5|7|1 for a normal link, 5|7|2 for a sticky link, and 5|7|3 for both links.",
			{
				"update link set style=2 where style=1",
				"update link set style=1 where style=0",
				"insert into link select from_id, to_id, sum(style) from link group by from_id, to_id",
				"delete from link where rowid not in (select rowid from link group by from_id, to_id having max(style))",
			}
		},
		{
			/* Upgrade to version 4 */
			"A 'sym' column has been added to the node table so symlinks can reference their destination nodes. This is necessary in order to properly handle dependencies on symlinks in an efficient manner.",
			{
				"alter table node add column sym integer default -1",
			}
		},
		{
			/* Upgrade to version 5 */
			"This is a pretty minor update - the link_index is adjusted to use (from_id, to_id) instead of just (from_id). This greatly improves the performance of link insertion, since a query has to be done for uniqueness and style constraints.",
			{
				"drop index link_index",
				"create index link_index on link(from_id, to_id)",
			}
		},
		{
			/* Upgrade to version 6 */
			"Another minor update - just adding an index on node.sym so it can be quickly determined if a deleted node needs to be made into a ghost.",
			{
				"create index node_sym_index on node(sym)",
			}
		},
		{
			/* Upgrade to version 7 */
			"This includes a ghost_list for storing ghost ids so they can later be raptured.",
			{
				"create table ghost_list (id integer primary key not null)",
			}
		},
		{
			/* Upgrade to version 8 */
			"This is really the same as version 6. Turns out putting the ghost_list on disk was kinda stupid. Now it's all handled in a temporary table in memory during a transaction.",
			{
				"drop table ghost_list",
			}
		},
		{
			/* Upgrade to version 9 */
			"This version includes a per-file timestamp in order to determine if a file has changed in between monitor invocations, or during a scan. Note that since no mtimes currently exist in the database, this will cause all commands to be executed for the next update.",
			{
				"alter table node add column mtime integer default -1",
			}
		},
		{
			/* Upgrade to version 10 */
			"A new unique constraint was placed on the link table.",
			{
				"create table link_new (from_id integer, to_id integer, style integer, unique(from_id, to_id))",
				"insert or ignore into link_new select from_id, to_id, style from link",
				"drop index link_index",
				"drop index link_index2",
				"drop table link",
				"alter table link_new rename to link",
				"create index link_index2 on link(to_id)",
			}
		},
		{
			/* Upgrade to version 11 */
			"The delete_list is no longer necessary, and is now gone.",
			{
				"drop table delete_list",
			}
		},
		{
			/* Upgrade to version 12 */
			"Extraneous ghosts were removed, and a new unique constraint was placed on the node table.",
			{
				/* First clear out any ghosts that shouldn't be
				 * there.  We don't want them to take
				 * precedence over real nodes that may have
				 * been moved over them.
				 */
				"delete from node where id in (select id from node as node1 where type=5 and exists(select id from node as node2 where node1.name=node2.name and node1.dir=node2.dir and node2.type!=5))",
				"drop index node_dir_index",
				"create table node_new (id integer primary key not null, dir integer not null, type integer not null, sym integer not null, mtime integer not null, name varchar(4096), unique(dir, name))",
				"insert or ignore into node_new select id, dir, type, sym, mtime, name from node",
				"drop index node_sym_index",
				"drop table node",
				"alter table node_new rename to node",
				"create index node_sym_index on node(sym)",
			}
		},
		{
			/* Upgrade to version 13 */
			"The sym field was removed, since symlinks are automatically handled by the filesystem layer.",
			{
				"create table node_new (id integer primary key not null, dir integer not null, type integer not null, mtime integer not null, name varchar(4096), unique(dir, name))",
				"insert or ignore into node_new select id, dir, type, mtime, name from node",
				"drop index node_sym_index",
				"drop table node",
				"alter table node_new rename to node",
			}
		},
		{
			/* Upgrade to version 14 */
			"A new config_list table was added to handle tup.config changes for variants.",
			{
				"insert or ignore into node(dir, name, type, mtime) values(1, 'tup.config', 5, -1)",
				"create table config_list (id integer primary key not null)",
				"create table variant_list (id integer primary key not null)",
				"alter table node add column srcid integer default -1",
				"update node set dir=(select id from node where name='tup.config' and dir=1) where dir=2",
				"update create_list set id=(select id from node where name='tup.config' and dir=1) where id=2",
				"update modify_list set id=(select id from node where name='tup.config' and dir=1) where id=2",
				"delete from node where id=2",
			}
		},
		{
			/* Upgrade to version 15 */
			"The link table was split to better handle groups and simplify logic. All Tupfiles will be re-parsed to add groups using the new tables.",
			{
				"create table normal_link (from_id integer, to_id integer, unique(from_id, to_id))",
				"create table sticky_link (from_id integer, to_id integer, unique(from_id, to_id))",
				"create table group_link (from_id integer, to_id integer, cmdid integer, unique(from_id, to_id, cmdid))",
				"create index normal_index2 on normal_link(to_id)",
				"create index sticky_index2 on sticky_link(to_id)",
				"create index group_index2 on group_link(cmdid)",
				"insert into normal_link select from_id, to_id from link where style=1 or style=3",
				"insert into sticky_link select from_id, to_id from link where style=2 or style=3",
				"delete from sticky_link where from_id in (select id from node where type=6)",
				"delete from sticky_link where to_id in (select id from node where type=6)",
				"delete from node where type=6",
				"drop table link",
				"insert or replace into create_list select id from node where type=2",
			}
		},
		{
			/* Upgrade to version 16 */
			"Added an index for node.srcid",
			{
				"create index srcid_index on node(srcid)",
				"update node set srcid=dir where type=4",
			}
		},
		{
			/* Upgrade to version 17 */
			"Added display and flags columns. All Tupfiles will be reparsed.",
			{
				"alter table node add column display varchar(4096)",
				"alter table node add column flags varchar(256)",
				"insert or replace into create_list select id from node where type=2",
			}
		},
		{
			/* Upgrade to version 18 */
			"Added a transient list for outputs that are deleted after they are no longer used.",
			{
				"create table transient_list (id integer primary key not null)",
			}
		},
		{
			/* Upgrade to version 19 */
			"Added an mtime_ns column for nanosecond timestamp resolution.",
			{
				"alter table node add column mtime_ns integer default 0",
			}
		},
	};

	if(tup_db_config_get_int("db_version", -1, &version) < 0)
		return -1;
	if(version < 0) {
		fprintf(stderr, "Error getting .tup/db version.\n");
		return -1;
	}

	if(version > DB_VERSION) {
		fprintf(stderr, "tup error: database is version %i, but this version of tup (%s) can only handle up to %i.\n", version, tup_version, DB_VERSION);
		return -1;
	}
	if(version != DB_VERSION) {
		if(DB_VERSION > ARRAY_SIZE(upgrades)) {
			fprintf(stderr, "tup internal error: Trying to upgrade to db version %i, but there are only upgrades available up to %i\n", DB_VERSION, ARRAY_SIZE(upgrades));
			return -1;
		}
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

		for(x=version; x<ARRAY_SIZE(upgrades); x++) {
			for(y=0; y<MAX_UPGRADE; y++) {
				if(upgrades[x].statements[y] == NULL)
					break;
				if(sqlite3_exec(tup_db, upgrades[x].statements[y], NULL, NULL, &errmsg) != 0) {
					fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
						errmsg, upgrades[x].statements[y]);
					return -1;
				}
			}
			if(tup_db_config_set_int("db_version", x + 1) < 0)
				return -1;
			printf("WARNING: Tup database upgraded to version %i.\n%s\n\n", x+1, upgrades[x].message);
		}
		if(tup_db_commit() < 0)
			return -1;
		if(tup_db_begin() < 0)
			return -1;
		printf("Database update successful. You can remove the backup database file in the .tup/ directory if everything appears to be working.\n");
	}

	if(tup_db_config_get_int("parser_version", 0, &version) < 0)
		return -1;
	if(version < 0) {
		fprintf(stderr, "Error getting tup parser version.\n");
		return -1;
	}
	if(version != PARSER_VERSION) {
		printf("Tup parser version has been updated to %i. All Tupfiles will be re-parsed to ensure that nothing broke.\n", PARSER_VERSION);
		if(tup_db_reparse_all() < 0)
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

/* This counts the database changes that are "expected" during refactoring,
 * such as adding/removing directory level dependencies, or removing the
 * create flag.
 */
static int expected_changes = 0;
int tup_db_changes(void)
{
	return sqlite3_total_changes(tup_db) - expected_changes;
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
	char s4[] = "select * from transient_list";

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
		transaction_started = 0;
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
		transaction_started = 0;
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
		transaction_started = 0;
	}
	if(flags & TUP_FLAGS_TRANSIENT) {
		transaction_check("%s", s4);
		check_flags_name = "transient";
		if(sqlite3_exec(tup_db, s4, check_flags_cb, &rc, &errmsg) != 0) {
			fprintf(stderr, "SQL select error: %s\nQuery was: %s\n",
				errmsg, s4);
			sqlite3_free(errmsg);
			return -1;
		}
		transaction_started = 0;
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
	struct tent_entries root = TENT_ENTRIES_INITIALIZER;
	reclaim_ghost_debug = 1;

	/* First get all ghosts loaded */
	if(tup_db_type_to_tree(&root, TUP_NODE_GHOST) < 0)
		return -1;
	/* Free the tmp tree, since the ghost check actually is done by
	 * ghost_root in tup_db_commit().
	 */
	free_tent_tree(&root);

	if(tup_entry_debug_add_all_ghosts(&ghost_root) < 0)
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
		case TUP_NODE_GROUP:
			str = "group";
			break;
		case TUP_NODE_GENERATED_DIR:
			str = "generated directory";
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

struct tup_entry *tup_db_create_node(struct tup_entry *dtent, const char *name, enum TUP_NODE_TYPE type)
{
	return tup_db_create_node_part(dtent, name, -1, type, -1, NULL);
}

struct tup_entry *tup_db_create_node_srcid(struct tup_entry *dtent, const char *name, enum TUP_NODE_TYPE type, tupid_t srcid,
					   int *node_changed)
{
	return tup_db_create_node_part(dtent, name, -1, type, srcid, node_changed);
}

struct tup_entry *tup_db_create_node_part(struct tup_entry *dtent, const char *name, int len,
					  enum TUP_NODE_TYPE type, tupid_t srcid, int *node_changed)
{
	return tup_db_create_node_part_display(dtent, name, len, NULL, 0, NULL, 0, type, srcid, node_changed);
}

struct tup_entry *tup_db_create_node_part_display(struct tup_entry *dtent, const char *name, int namelen,
						  const char *display, int displaylen, const char *flags, int flagslen,
						  enum TUP_NODE_TYPE type, tupid_t srcid, int *node_changed)
{
	struct tup_entry *tent;

	if(node_select(dtent, name, namelen, &tent) < 0) {
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

			if(type != TUP_NODE_GHOST) {
				if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
					return NULL;
				if(tup_db_set_type(tent, type) < 0)
					return NULL;
				if(tup_db_set_srcid(tent, srcid) < 0)
					return NULL;
				if(node_changed)
					*node_changed = 1;
			}

			{
				struct variant *variant;
				/* If the variant dir was converted from a
				 * ghost, that means we rm'd it, scanned
				 * without processing config nodes, and
				 * re-created it. In this case we want to
				 * disable the variant so that we know we need
				 * to re-parse this variant in the updater.
				 */
				variant = variant_search(tent->tnode.tupid);
				if(variant) {
					if(variant_rm(variant) < 0)
						return NULL;
				}
			}
			return tent;
		}
		if(tent->type != type) {
			/* If we have a generated dir in the tup db, the
			 * scanner will report TUP_NODE_DIR. In this case we
			 * don't want to change the type.
			 */
			if(! (tent->type == TUP_NODE_GENERATED_DIR && type == TUP_NODE_DIR)) {
				/* If we changed from one type to another (eg: a file
				 * became a directory), then delete the old one and
				 * create a new one.
				 */
				if(tup_del_id_force(tent->tnode.tupid, tent->type) < 0)
					return NULL;

				/* Even after a delete, we may just now be a
				 * ghost, since non-directories can have
				 * ghosts attached to them in the ldpreload
				 * server. So call back into this function to
				 * get potential ghost handling (t6081,
				 * t6082).
				 */
				return tup_db_create_node_part_display(dtent, name, namelen,
								       display, displaylen,
								       flags, flagslen,
								       type, srcid, node_changed);
			}
		}
		/* During the initial scan, srcid will be -1 so we don't want
		 * to overwrite in this case. However, we do want to overwrite
		 * if we are setting srcid (eg: a directory may exist in the
		 * variant without a corresponding node in the db if the parser
		 * failed). In this case we will create the node during the
		 * initial scan, then overwrite the srcid later.
		 */
		if(srcid != -1 && tent->srcid != srcid) {
			if(tup_db_set_srcid(tent, srcid) < 0)
				return NULL;
		}

		/* Windows has case-insensitive matching, but we want
		 * to update the name if it has changed.
		 */
		if(strcmp(name, tent->name.s) != 0) {
			if(tup_db_set_name(tent->tnode.tupid, name, tent->dt) < 0)
				return NULL;
			if(node_changed)
				*node_changed = 1;
		}

		/* Commands never go through here - if the tent already exists,
		 * it is handled in the parser. So we don't need to check &
		 * update the 'display' field.
		 */
		if(tent->displaylen != displaylen || (displaylen > 0 && strncmp(tent->display, display, displaylen) != 0)) {
			fprintf(stderr, "tup internal error: 'display' field shouldn't be changing here: %.*s -> %.*s\n", tent->displaylen, tent->display, displaylen, display);
			return NULL;
		}

		/* We do need to check the flags field, since files can have
		 * the transient flag and may go from normal to transient or
		 * vice versa.
		 */
		if(tent->flagslen != flagslen || (flagslen > 0 && strncmp(tent->flags, flags, flagslen) != 0)) {
			if(tup_db_set_flags(tent, flags, flagslen) < 0)
				return NULL;
		}
		return tent;
	}

	tent = node_insert(dtent, name, namelen, display, displaylen, flags, flagslen, type, INVALID_MTIME, srcid);
	if(node_changed)
		*node_changed = 1;
	return tent;
}

int tup_db_fill_tup_entry(tupid_t tupid, struct tup_entry **dest)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_FILL_TUP_ENTRY];
	static char s[] = "select dir, type, mtime, mtime_ns, srcid, name, display, flags from node where id=?";
	tupid_t dt;
	enum TUP_NODE_TYPE type;
	struct timespec mtime;
	tupid_t srcid;
	const char *name;
	const char *display;
	const char *flags;

	if(tupid == 0) {
		/* Create a dummy tup_entry for DOT_DT's parent, so we can
		 * tup_entry_add(tupid == 0) and get a valid pointer.
		 */
		if(tup_entry_add_all(0, 0, TUP_NODE_ROOT, INVALID_MTIME, -1, "{root}", "{root}", "", dest) < 0) {
			return -1;
		}
		return 0;
	}

	transaction_check("%s [%lli]", s, tupid);
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

	dt = sqlite3_column_int64(*stmt, 0);
	type = sqlite3_column_int(*stmt, 1);
	mtime.tv_sec = sqlite3_column_int(*stmt, 2);
	mtime.tv_nsec = sqlite3_column_int(*stmt, 3);
	srcid = sqlite3_column_int64(*stmt, 4);
	name = (const char*)sqlite3_column_text(*stmt, 5);
	display = (const char*)sqlite3_column_text(*stmt, 6);
	flags = (const char*)sqlite3_column_text(*stmt, 7);

	if(tup_entry_add_all(tupid, dt, type, mtime, srcid, name, display, flags, dest) < 0)
		goto out_reset;

	rc = 0;

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	return rc;
}

int tup_db_select_tent(struct tup_entry *dtent, const char *name, struct tup_entry **entry)
{
	return node_select(dtent, name, -1, entry);
}

int tup_db_select_tent_part(struct tup_entry *dtent, const char *name, int len,
			    struct tup_entry **entry)
{
	return node_select(dtent, name, len, entry);
}

int tup_db_select_node_by_flags(int (*callback)(void *, struct tup_entry *),
				void *arg, int flags)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt;
	static char s1[] = "select * from config_list";
	static char s2[] = "select * from create_list";
	static char s3[] = "select * from modify_list";
	static char s4[] = "select * from variant_list";
	static char s5[] = "select * from transient_list";
	char *sql;
	int sqlsize;
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);

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
	} else if(flags == TUP_FLAGS_VARIANT) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_4];
		sql = s4;
		sqlsize = sizeof(s4);
	} else if(flags == TUP_FLAGS_TRANSIENT) {
		stmt = &stmts[DB_SELECT_NODE_BY_FLAGS_5];
		sql = s5;
		sqlsize = sizeof(s5);
	} else {
		fprintf(stderr, "tup error: tup_db_select_node_by_flags() must specify exactly one of TUP_FLAGS_CONFIG/TUP_FLAGS_CREATE/TUP_FLAGS_MODIFY/TUP_FLAGS_VARIANT\n");
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

		if(tupid_list_add_tail(&tupid_list, sqlite3_column_int64(*stmt, 0)) < 0) {
			rc = -1;
			goto out_reset;
		}
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;

			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(callback(arg, tent) < 0) {
				return -1;
			}
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

int tup_db_select_node_dir(int (*callback)(void *, struct tup_entry *),
			   void *arg, tupid_t dt)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_DIR];
	static char s[] = "select id from node where dir=?";
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);
	transaction_check("%s [%lli]", s, dt);
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

		if(tupid_list_add_tail(&tupid_list, sqlite3_column_int64(*stmt, 0)) < 0) {
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

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;

			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(callback(arg, tent) < 0)
				return -1;
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

int tup_db_select_node_dir_glob(int (*callback)(void *, struct tup_entry *),
				void *arg, struct tup_entry *dtent,
				const char *glob, int len,
				struct tent_entries *delete_root,
				int include_directories)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_DIR_GLOB];
	static char s[] = "select id, name, type, mtime, mtime_ns, srcid, display, flags from node where dir=? and (type=? or type=? or type=?) and name glob ?" SQL_NAME_COLLATION;
	int extra_type;

	if(include_directories) {
		extra_type = TUP_NODE_DIR;
	} else {
		/* We already specify TUP_NODE_GENERATED, but it doesn't really
		 * hurt to specify it again in the 'or type=?' part.
		 */
		extra_type = TUP_NODE_GENERATED;
	}

	transaction_check("%s [%lli, %i, %i, %i, '%s']", s, dtent->tnode.tupid, TUP_NODE_FILE, TUP_NODE_GENERATED, extra_type, glob);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dtent->tnode.tupid) != 0) {
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
	if(sqlite3_bind_int(*stmt, 4, extra_type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 5, glob, len, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	while(1) {
		struct tup_entry *tent;
		tupid_t tupid;
		const char *name;
		const char *display;
		const char *flags;
		enum TUP_NODE_TYPE type;
		struct timespec mtime;
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
		if(tent_tree_search_tupid(delete_root, tupid) == NULL) {
			tent = tup_entry_find(tupid);
			if(!tent) {
				name = (const char *)sqlite3_column_text(*stmt, 1);
				type = sqlite3_column_int(*stmt, 2);
				mtime.tv_sec = sqlite3_column_int64(*stmt, 3);
				mtime.tv_nsec = sqlite3_column_int64(*stmt, 4);
				srcid = sqlite3_column_int64(*stmt, 5);
				display = (const char *)sqlite3_column_text(*stmt, 6);
				flags = (const char *)sqlite3_column_text(*stmt, 7);

				if(tup_entry_add_to_dir(dtent, tupid, name, -1, display, -1, flags, -1, type, mtime, srcid, &tent) < 0) {
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

static int rm_generated_dir(struct tup_entry *tent)
{
	if(tent->type == TUP_NODE_GENERATED_DIR) {
		int dfd;
		dfd = tup_entry_open(tent->parent);
		if(dfd < 0) {
			fprintf(stderr, "tup error: Unable to delete generated directory: ");
			print_tup_entry(stderr, tent);
			fprintf(stderr, "\n");
			return -1;
		}
		if(unlinkat(dfd, tent->name.s, AT_REMOVEDIR) < 0) {
			if(errno != ENOENT) {
				perror(tent->name.s);
				fprintf(stderr, "tup error: Unable to delete generated directory: ");
				print_tup_entry(stderr, tent);
				fprintf(stderr, "\n");
				return -1;
			}
		}
		if(close(dfd) < 0) {
			perror("close(dfd)");
			return -1;
		}
	}
	return 0;
}

int tup_db_delete_node(tupid_t tupid)
{
	int rc;
	struct tup_entry *tent;
	struct tup_entry *parent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	parent = tent->parent;

	if(tent->srcid >= 0) {
		struct tup_entry *srctent;
		/* We may need to remove the directory that created us if it
		 * was also removed.
		 */
		if(tup_entry_add(tent->srcid, &srctent) < 0)
			return -1;
		if(tup_entry_add_ghost_tree(&ghost_root, srctent) < 0)
			return -1;
	}

	rc = node_has_ghosts(tupid);
	if(rc < 0)
		return -1;
	if(rc == 1) {
		/* We're but a ghost now. This can happen if a directory is
		 * removed that contains a ghost (t6061).
		 */
		if(tent->type == TUP_NODE_DIR) {
			/* Make sure we "reparse" the directory so that files
			 * generated in other directories can be properly
			 * cleaned up.
			 */
			if(tup_db_add_create_list(tent->tnode.tupid) < 0)
				return -1;
		}
		if(tup_db_set_type(tent, TUP_NODE_GHOST) < 0)
			return -1;
		if(tup_db_set_srcid(tent, -1) < 0)
			return -1;
		return 0;
	}

	if(rm_generated_dir(tent) < 0)
		return -1;

	if(delete_node(tupid) < 0)
		return -1;
	if(tup_entry_add_ghost_tree(&ghost_root, parent) < 0)
		return -1;

	return 0;
}

int delete_node(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_DELETE_NODE];
	static char s[] = "delete from node where id=?";

	if(tup_entry_rm(tupid) < 0) {
		return -1;
	}

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_delete_dir(tupid_t dt, int force)
{
	struct half_entry_head subdir_list;

	/* If we remove a directory, we need to re-parse any Tupfiles that
	 * write files to this directory so that this node can potentially be
	 * recreated as a generated directory (t4195) or report an error if
	 * this node is no longer a directory (t4196).
	 */
	if(tup_db_set_srcid_dir_flags(dt) < 0)
		return -1;

	LIST_INIT(&subdir_list);
	if(get_dir_entries(dt, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);

		if(he->type == TUP_NODE_DIR) {
			/* Pass the force flag along to removed sub-directories
			 * so that variants can be handled properly (t8034).
			 * This will call back to tup_db_delete_dir().
			 */
			if(tup_del_id_type(he->tupid, he->type, force, NULL) < 0)
				return -1;
		} else {
			if(tup_del_id_force(he->tupid, he->type) < 0)
				return -1;
		}
		LIST_REMOVE(he, list);
		free(he);
	}

	return 0;
}

int tup_db_flag_generated_dir(tupid_t dt, int force)
{
	struct half_entry_head subdir_list;

	if(force) {
		struct tup_entry *tent;

		if(tup_entry_add(dt, &tent) < 0)
			return -1;
		/* We only need to re-parse other Tupfiles that write to this
		 * directory when force-removed. If the directory is missing
		 * we can just make sure that the command is scheduled to be
		 * executed in the while loop below.
		 */
		if(tup_db_set_srcid_dir_flags(tent->tnode.tupid) < 0)
			return -1;

		/* When we're force removed, set this node to be a ghost
		 * temporarily so that we don't try to call rmdir on it as if
		 * it were a generated directory.
		 */
		if(tup_db_set_type(tent, TUP_NODE_GHOST) < 0)
			return -1;
		if(tup_entry_add_ghost_tree(&ghost_root, tent) < 0)
			return -1;
	}

	LIST_INIT(&subdir_list);
	if(get_dir_entries(dt, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);

		if(tup_del_id_type(he->tupid, he->type, force, NULL) < 0)
			return -1;
		LIST_REMOVE(he, list);
		free(he);
	}

	return 0;
}

static int delete_variant_dir(struct tup_entry *tent, void *arg, int (*callback)(void *arg, struct tup_entry *tent))
{
	struct half_entry_head subdir_list;
	int fd;

	if(callback)
		if(callback(arg, tent) < 0)
			return -1;

	LIST_INIT(&subdir_list);
	if(get_dir_entries(tent->tnode.tupid, &subdir_list) < 0)
		return -1;
	fd = tup_entry_open(tent);
	if(fd < 0) {
		/* If we can't open the directory, it must already be gone.
		 * Just go through a normal tup_db_delete_dir().
		 */
		if(errno == ENOENT) {
			if(tup_db_delete_dir(tent->tnode.tupid, 0) < 0)
				return -1;
			return 0;
		}
		fprintf(stderr, "tup error: Unable to open directory for variant removal: ");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return -1;
	}
	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);
		struct tup_entry *subtent;
		int flags = -1;

		if(tup_entry_add(he->tupid, &subtent) < 0)
			return -1;

		if(he->type == TUP_NODE_DIR) {
			flags = AT_REMOVEDIR;
			if(delete_variant_dir(subtent, arg, callback) < 0)
				return -1;
		} else if(he->type == TUP_NODE_GENERATED) {
			flags = 0;
		}

		if(flags >= 0) {
			if(unlinkat(fd, subtent->name.s, flags) < 0) {
				if(errno != ENOENT) {
					perror(subtent->name.s);
					fprintf(stderr, "tup error: Unable to clean out variant directory: ");
					print_tup_entry(stderr, tent);
					fprintf(stderr, "\ntup error: Please make sure you have no extra files in the variant directory.\n");
					return -1;
				}
			}
		}
		if(delete_name_file(he->tupid) < 0)
			return -1;

		LIST_REMOVE(he, list);
		free(he);
	}
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}

	return 0;
}

int tup_db_delete_variant(struct tup_entry *tent, void *arg, int (*callback)(void *arg, struct tup_entry *tent))
{
	if(tent->tnode.tupid == DOT_DT) {
		fprintf(stderr, "tup internal error: Shouldn't be trying to clean up the root variant. Tup entry is: ");
		print_tup_entry(stderr, tent);
		fprintf(stderr, "\n");
		return -1;
	}

	if(delete_variant_dir(tent, arg, callback) < 0)
		return -1;
	return 0;
}

static int recurse_delete_ghost_tree(tupid_t tupid, int modify)
{
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

	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);
		if(he->type != TUP_NODE_GHOST) {
			fprintf(stderr, "tup internal error: Why does a node of type %i have a ghost dir?\n", he->type);
			tup_db_print(stderr, he->tupid);
			return -1;
		}
		if(recurse_delete_ghost_tree(he->tupid, modify) < 0)
			return -1;
		if(delete_node(he->tupid) < 0)
			return -1;
		LIST_REMOVE(he, list);
		free(he);
	}
	return 0;
}

static int recurse_modify_dir(tupid_t dt)
{
	struct half_entry_head subdir_list;

	if(tup_db_add_create_list(dt) < 0)
		return -1;

	LIST_INIT(&subdir_list);
	if(get_dir_entries(dt, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);
		if(he->type == TUP_NODE_DIR) {
			if(recurse_modify_dir(he->tupid) < 0)
				return -1;
		} else {
			if(he->type == TUP_NODE_GENERATED || he->type == TUP_NODE_FILE)
				if(tup_db_add_modify_list(he->tupid) < 0)
					return -1;
			if(tup_db_set_dependent_flags(he->tupid) < 0)
				return -1;
			if(tup_db_maybe_add_config_list(he->tupid) < 0)
				return -1;
		}
		LIST_REMOVE(he, list);
		free(he);
	}

	return 0;
}

static int duplicate_directory_structure(int fd, struct tup_entry *dest, struct tup_entry *src,
					 struct tup_entry *destroot)
{
	struct tent_list *tl;
	struct tent_list_head subdir_list;

	tent_list_init(&subdir_list);
	if(tup_db_dirtype(src->tnode.tupid, &subdir_list, NULL, TUP_NODE_DIR) < 0)
		return -1;
	tent_list_foreach(tl, &subdir_list) {
		struct tup_entry *subdest;
		struct tup_entry *subsrc;
		int newfd;

		subsrc = tl->tent;
		if(subsrc == destroot)
			continue;
		if(is_virtual_tent(subsrc))
			continue;
		if(tup_entry_variant(subsrc)->tent->dt != DOT_DT)
			continue;

		if(mkdirat(fd, subsrc->name.s, 0777) < 0) {
			if(errno != EEXIST) {
				perror(subsrc->name.s);
				fprintf(stderr, "tup error: Unable to create sub-directory in variant tree.\n");
				return -1;
			}
		}
		subdest = tup_db_create_node_srcid(dest, subsrc->name.s, TUP_NODE_DIR, subsrc->tnode.tupid, NULL);
		if(!subdest) {
			fprintf(stderr, "tup error: Unable to create tup node for variant directory: ");
			print_tup_entry(stderr, subdest);
			fprintf(stderr, "\n");
			return -1;
		}

		if(tup_db_add_create_list(subdest->tnode.tupid) < 0)
			return -1;

		newfd = openat(fd, subdest->name.s, O_RDONLY);
		if(newfd < 0) {
			perror(subdest->name.s);
			fprintf(stderr, "tup error: Unable to open subdirectory while duplicating the directory structure.\n");
			return -1;
		}
		if(duplicate_directory_structure(newfd, subdest, subsrc, destroot) < 0)
			return -1;
		if(close(newfd) < 0) {
			perror("close(newfd)");
			return -1;
		}
	}
	free_tent_list(&subdir_list);
	return 0;
}

int tup_db_duplicate_directory_structure(struct tup_entry *dest)
{
	int fd;
	int rc;
	struct tup_entry *root_tent;

	fd = tup_entry_open(dest);
	if(fd < 0)
		return -1;
	if(tup_entry_add(DOT_DT, &root_tent) < 0)
		return -1;
	rc = duplicate_directory_structure(fd, dest, root_tent, dest);
	if(close(fd) < 0) {
		perror("close(fd)");
		return -1;
	}
	return rc;
}

int tup_db_chdir(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_CHDIR];
	static char s[] = "select dir, name from node where id=?";
	tupid_t parent;
	char *path;

	if(tupid == 0) {
		fprintf(stderr, "tup error: Trying to tup_db_chdir(0)\n");
		return -1;
	}
	if(tupid == 1) {
		return chdir(get_tup_top());
	}
	transaction_check("%s [%lli]", s, tupid);
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

	if(tup_db_chdir(parent) < 0) {
		free(path);
		return -1;
	}

	rc = chdir(path);
	if(rc < 0) {
		if(errno == ENOENT)
			rc = -ENOENT;
		else
			perror(path);
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

int tup_db_change_node(tupid_t tupid, const char *new_name, struct tup_entry *new_dtent)
{
	struct tup_entry *tent;

	if(node_select(new_dtent, new_name, -1, &tent) < 0) {
		return -1;
	}
	if(tent) {
		if(tent->type == TUP_NODE_GHOST) {
			if(recurse_delete_ghost_tree(tent->tnode.tupid, 1) < 0)
				return -1;
			if(delete_node(tent->tnode.tupid) < 0)
				return -1;
		} else {
			fprintf(stderr, "tup error: Attempting to overwrite node '%s' in dir %lli in tup_db_change_node()\n", new_name, new_dtent->tnode.tupid);
			tup_db_print(stderr, new_dtent->tnode.tupid);
			return -1;
		}
	}

	if(tup_db_set_name(tupid, new_name, new_dtent->tnode.tupid) < 0)
		return -1;
	if(recurse_modify_dir(tupid) < 0)
		return -1;
	return 0;
}

int tup_db_set_name(tupid_t tupid, const char *new_name, tupid_t new_dt)
{
	int rc;
	struct tup_entry *tent;
	sqlite3_stmt **stmt = &stmts[DB_SET_NAME];
	static char s[] = "update node set name=?, dir=? where id=?";

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	if(strcmp(tent->name.s, new_name) == 0 && tent->dt == new_dt)
		return 0;

	transaction_check("%s ['%s', %lli, %lli]", s, new_name, new_dt, tupid);
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

int tup_db_set_display(struct tup_entry *tent, const char *display, int displaylen)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_DISPLAY];
	static char s[] = "update node set display=? where id=?";

	transaction_check("%s ['%.*s', %lli]", s, displaylen, display, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, display, displaylen, SQLITE_STATIC) != 0) {
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

	if(tup_entry_change_display(tent, display, displaylen) < 0)
		return -1;

	return 0;
}

int tup_db_set_flags(struct tup_entry *tent, const char *flags, int flagslen)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_FLAGS];
	static char s[] = "update node set flags=? where id=?";

	transaction_check("%s ['%.*s', %lli]", s, flagslen, flags, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_text(*stmt, 1, flags, flagslen, SQLITE_STATIC) != 0) {
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

	if(tup_entry_change_flags(tent, flags, flagslen) < 0)
		return -1;

	return 0;
}

int tup_db_set_type(struct tup_entry *tent, enum TUP_NODE_TYPE type)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_TYPE];
	static char s[] = "update node set type=? where id=?";

	transaction_check("%s [%i, %lli]", s, type, tent->tnode.tupid);
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

int tup_db_set_mtime(struct tup_entry *tent, struct timespec mtime)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_MTIME];
	static char s[] = "update node set mtime=?, mtime_ns=? where id=?";

	if(!tent) {
		fprintf(stderr, "tup internal error: tent is NULL in tup_db_set_mtime()\n");
		return -1;
	}

	transaction_check("%s [%li, %lli, %lli]", s, mtime.tv_sec, mtime.tv_nsec, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, mtime.tv_sec) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, mtime.tv_nsec) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 3, tent->tnode.tupid) != 0) {
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

int tup_db_set_srcid(struct tup_entry *tent, tupid_t srcid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_SRCID];
	static char s[] = "update node set srcid=? where id=?";

	transaction_check("%s [%lli, %lli]", s, srcid, tent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, srcid) != 0) {
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

	tent->srcid = srcid;
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

static int write_gitignore_line(FILE * f, const unsigned char *to_ignore)
{
	const size_t str_size = PATH_MAX + 1;
	char *const str = calloc(str_size, sizeof(*str));

	if (str == NULL) {
		perror("calloc");
		goto abort;
	}

	size_t i = 0;
	for (; *to_ignore != '\0'; ++i, ++to_ignore) {
		const char c = *to_ignore;

		switch (c) {
		case '[':
		case ']':
			if (i >= str_size)
				goto abort;
			str[i] = '\\';
			++i;
		}

		if (i >= str_size)
			goto abort;
		str[i] = c;
	}

	if (fprintf(f, "/%s\n", str) < 0) {
		perror("fprintf");
		goto abort;
	}

	free(str);
	return 0;

 abort:
	free(str);
	return -1;
}

int tup_db_write_gitignore(FILE *f, tupid_t dt, int skip_self)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_WRITE_GITIGNORE];
	static char s[] = "select name from node where dir=? and (type=? or type=?)";

	transaction_check("%s [%lli, %i, %i]", s, dt, TUP_NODE_GENERATED, TUP_NODE_GENERATED_DIR);
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
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

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

		const char *filename = (const char *)sqlite3_column_text(*stmt, 0);
		if(!skip_self || strcmp(filename, ".gitignore") != 0) {
			if(write_gitignore_line(f, sqlite3_column_text(*stmt, 0)) < 0) {
				fprintf(stderr, "tup error: Unable to write data to .gitignore file.\n");
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

int tup_db_rebuild_all(void)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_REBUILD_ALL];
	static char s[] = "insert or ignore into modify_list select id from node where type=?";

	transaction_check("%s [%i]", s, TUP_NODE_CMD);
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
	if(msqlite3_reset(*stmt) != 0) {
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

static int db_print(FILE *stream, tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_PRINT];
	static char s[] = "select dir, type, name from node where id=?";
	tupid_t parent;
	enum TUP_NODE_TYPE type;
	char *path;

	if(tupid == 0) {
		fprintf(stderr, "tup error: Trying to tup_db_print(0)\n");
		return -1;
	}
	if(tupid == 1) {
		return 0;
	}
	transaction_check("%s [%lli]", s, tupid);
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
		case TUP_NODE_GENERATED_DIR:
			fprintf(stream, "%s", path);
			break;
		case TUP_NODE_CMD:
			fprintf(stream, "[[34m%s[0m]", path);
			break;
		case TUP_NODE_GHOST:
			fprintf(stream, "[47;30m%s[0m", path);
			break;
		case TUP_NODE_GROUP:
			fprintf(stream, "[36m%s[0m", path);
			break;
		case TUP_NODE_FILE:
		case TUP_NODE_GENERATED:
		case TUP_NODE_VAR:
		case TUP_NODE_ROOT:
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

int tup_db_maybe_add_config_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MAYBE_ADD_CONFIG_LIST];
	static char s[] = "insert or ignore into config_list select id from variant_list where id=?";

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_maybe_add_create_list(tupid_t tupid)
{
	/* We only add a node to the create list if:
	 * 1) It exists in the node table
	 * 2) It is a directory (ie: not a ghost)
	 * This prevents nodes with a stale srcid from ending up in the create_list.
	 */
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MAYBE_ADD_CREATE_LIST];
	static char s[] = "insert or ignore into create_list select id from node where id=? and type=?";

	transaction_check("%s [%lli, %i]", s, tupid, TUP_NODE_DIR);
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

int tup_db_add_create_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_CREATE_LIST];
	static char s[] = "insert or ignore into create_list values(?)";

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_add_transient_list(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_ADD_TRANSIENT_LIST];
	static char s[] = "insert or ignore into transient_list values(?)";

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_in_create_list(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_IN_CREATE_LIST];
	static char s[] = "select id from create_list where id=?";

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_in_transient_list(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_IN_TRANSIENT_LIST];
	static char s[] = "select id from transient_list where id=?";

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

	/* Only factor in the change if we actually remove something. We call
	 * this for every node in the create graph, even dependent ones that
	 * aren't actually in the create list.
	 */
	expected_changes += sqlite3_changes(tup_db);
	return 0;
}

int tup_db_unflag_modify(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_MODIFY];
	static char s[] = "delete from modify_list where id=?";

	transaction_check("%s [%lli]", s, tupid);
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

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_unflag_transient(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_UNFLAG_TRANSIENT];
	static char s[] = "delete from transient_list where id=?";

	transaction_check("%s [%lli]", s, tupid);
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

static int get_dir_entries(tupid_t dt, struct half_entry_head *head)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_DIR_ENTRIES];
	static char s[] = "select id, type from node where dir=?";

	transaction_check("%s [%lli]", s, dt);
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
	if(link_insert(a, b, style) < 0)
		return -1;
	return 0;
}

static int get_output_group(tupid_t cmdid, struct tup_entry **tent)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_OUTPUT_GROUP];
	static char s[] = "select to_id from normal_link, node where from_id=? and to_id=node.id and node.type=?";
	tupid_t tupid = -1;

	*tent = NULL;

	transaction_check("%s [%lli]", s, cmdid, TUP_NODE_GROUP);
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
	if(sqlite3_bind_int64(*stmt, 2, TUP_NODE_GROUP) != 0) {
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
	tupid = sqlite3_column_int64(*stmt, 0);

	/* Do a quick double-check to make sure there isn't a duplicate link. */
	dbrc = sqlite3_step(*stmt);
	if(dbrc != SQLITE_DONE) {
		if(dbrc != SQLITE_ROW) {
			fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
		fprintf(stderr, "tup error: Node %lli is supposed to only have one output group, but multiple were found. The database is probably in a bad state. Sadness :(\n", cmdid);
		rc = -1;
		goto out_reset;
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0 && tupid != -1) {
		if(tup_entry_add(tupid, tent) < 0)
			return -1;
	}
	return rc;
}

int tup_db_create_unique_link(struct tup_entry *a, struct tup_entry *b)
{
	int rc;
	struct tup_entry *incoming;

	rc = tup_db_get_incoming_link(b, &incoming);
	if(rc < 0)
		return -1;
	if(incoming != NULL && incoming != a) {
		struct tup_entry *output_group = NULL;

		if(get_output_group(incoming->tnode.tupid, &output_group) < 0)
			return -1;
		if(output_group) {
			/* Make sure we remove the old group for the
			 * output (t3065)
			 */
			if(link_remove(b->tnode.tupid, output_group->tnode.tupid, TUP_LINK_NORMAL) < 0)
				return -1;
			if(tup_entry_add_ghost_tree(&ghost_root, output_group) < 0)
				return -1;
		}
		/* Delete any old links (t6029) */
		if(link_remove(incoming->tnode.tupid, b->tnode.tupid, TUP_LINK_NORMAL) < 0)
			return -1;

		b->incoming = NULL;
	} else if(incoming == NULL) {
		/* It's possible the output was orphaned when a command was
		 * wiped out because its source directory was removed, but the
		 * output can still point to its old group because the output
		 * lives in a different directory. We need to check for the old
		 * group and remove it (t6071).
		 */
		struct tup_entry *output_group = NULL;

		if(get_output_group(b->tnode.tupid, &output_group) < 0)
			return -1;
		if(output_group) {
			if(link_remove(b->tnode.tupid, output_group->tnode.tupid, TUP_LINK_NORMAL) < 0)
				return -1;
			if(tup_entry_add_ghost_tree(&ghost_root, output_group) < 0)
				return -1;
		}
	}
	return 0;
}

int tup_db_link_exists(tupid_t a, tupid_t b, int style,
		       int *exists)
{
	int rc;
	sqlite3_stmt **stmt;
	static char s1[] = "select to_id from normal_link where from_id=? and to_id=?";
	static char s2[] = "select to_id from sticky_link where from_id=? and to_id=?";
	char *sql;
	int sqlsize;

	if(style == TUP_LINK_NORMAL) {
		stmt = &stmts[DB_LINK_EXISTS1];
		sql = s1;
		sqlsize = sizeof(s1);
	} else if(style == TUP_LINK_STICKY) {
		stmt = &stmts[DB_LINK_EXISTS2];
		sql = s2;
		sqlsize = sizeof(s2);
	} else {
		fprintf(stderr, "tup error: incorrect style=%i in tup_db_link_exists()\n", style);
		return -1;
	}

	transaction_check("%s [%lli, %lli]", sql, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, sql, sqlsize, stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", sql);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}
	if(rc == SQLITE_DONE) {
		*exists = 0;
		return 0;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	*exists = 1;
	return 0;
}

int tup_db_get_incoming_link(struct tup_entry *tent, struct tup_entry **incoming)
{
	struct tent_entries root = TENT_ENTRIES_INITIALIZER;
	struct tent_tree *tt;
	struct tup_entry *incoming_tent = NULL;
	int set = 0;
	int rc = 0;

	*incoming = NULL;

	if(tent->incoming) {
		*incoming = tent->incoming;
		return 0;
	}

	if(get_normal_inputs(tent->tnode.tupid, &root, 0) < 0)
		return -1;

	RB_FOREACH(tt, tent_entries, &root) {
		if(!set) {
			set = 1;
			incoming_tent = tt->tent;
		} else {
			fprintf(stderr, "tup error: Node %lli is supposed to only have one incoming link, but multiple were found. The database is probably in a bad state. Sadness :(\n", tent->tnode.tupid);
			rc = -1;
			break;
		}
	}
	free_tent_tree(&root);

	if(rc == 0 && incoming_tent) {
		tent->incoming = incoming_tent;
		*incoming = tent->incoming;
	}

	return rc;
}

static int delete_normal_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_DELETE_NORMAL_LINKS];
	static const char s[] = "delete from normal_link where from_id=? or to_id=?";

	transaction_check("%s [%lli, %lli]", s, tupid, tupid);
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

static int delete_normal_inputs(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_DELETE_NORMAL_INPUTS];
	static const char s[] = "delete from normal_link where to_id=?";

	transaction_check("%s [%lli]", s, tupid);
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

static int delete_sticky_links(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_DELETE_STICKY_LINKS];
	static const char s[] = "delete from sticky_link where from_id=? or to_id=?";

	sticky_count++;

	transaction_check("%s [%lli, %lli]", s, tupid, tupid);
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

static int flag_group_users1(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_FLAG_GROUP_USERS1];
	static const char s[] = "insert or ignore into create_list select dir from node where id in (select to_id from sticky_link where from_id=?)";

	transaction_check("%s [%lli]", s, tupid);
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

static int flag_group_users2(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_FLAG_GROUP_USERS2];
	static const char s[] = "insert or ignore into create_list select dir from node where id in (select from_id from normal_link where to_id=?)";

	transaction_check("%s [%lli]", s, tupid);
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

static int flag_group_users(tupid_t tupid)
{
	struct tup_entry *tent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	if(tent->type == TUP_NODE_GROUP) {
		if(flag_group_users1(tupid) < 0)
			return -1;
		if(flag_group_users2(tupid) < 0)
			return -1;
	}
	return 0;
}

int tup_db_delete_links(tupid_t tupid)
{
	if(add_ghost_checks(tupid) < 0)
		return -1;
	if(add_group_and_exclusion_checks(tupid) < 0)
		return -1;
	if(flag_group_users(tupid) < 0)
		return -1;
	if(delete_normal_links(tupid) < 0)
		return -1;
	if(delete_sticky_links(tupid) < 0)
		return -1;
	if(delete_group_links(tupid) < 0)
		return -1;
	return 0;
}

int tup_db_normal_dir_to_generated(struct tup_entry *tent)
{
	if(add_ghost_checks(tent->tnode.tupid) < 0)
		return -1;
	if(delete_normal_links(tent->tnode.tupid) < 0)
		return -1;
	if(tup_db_set_type(tent, TUP_NODE_GENERATED_DIR) < 0)
		return -1;
	return 0;
}

int tup_db_dirtype(tupid_t dt, struct tent_list_head *head, struct tent_entries *root, enum TUP_NODE_TYPE type)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_DIRTYPE];
	static char s[] = "select id from node where dir=? and type=?";
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli, %i]", s, dt, type);
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
		if(tupid_list_add_tail(&tupid_list, tupid) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;
			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(head) {
				if(tent_list_add_tail(head, tent) < 0) {
					return -1;
				}
			}
			if(root) {
				if(tent_tree_add(root, tent) < 0) {
					return -1;
				}
			}
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

int tup_db_srcid_to_tree(tupid_t srcid, struct tent_entries *root, enum TUP_NODE_TYPE type)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SRCID_TO_TREE];
	static char s[] = "select id from node where srcid=? and type=?";
	struct tupid_list_head tupid_list;
	struct tupid_list *tl;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli, %i]", s, srcid, type);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, srcid) != 0) {
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
		if(tupid_list_add_tail(&tupid_list, tupid) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;
			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(tent_tree_add(root, tent) < 0)
				return -1;
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

int tup_db_type_to_tree(struct tent_entries *root, enum TUP_NODE_TYPE type)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_TYPE_TO_TREE];
	static char s[] = "select id from node where type=?";
	struct tupid_list_head tupid_list;
	struct tupid_list *tl;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%i]", s, type);
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
		if(tupid_list_add_tail(&tupid_list, tupid) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;
			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(tent_tree_add(root, tent) < 0)
				return -1;
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

static int is_generated_dir1(tupid_t dt)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_IS_GENERATED_DIR1];
	static char s[] = "select exists(select 1 from node where dir=? and (type=? or type=?))";

	transaction_check("%s [%lli, %i, %i]", s, dt, TUP_NODE_FILE, TUP_NODE_DIR);
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
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc == SQLITE_DONE) {
		fprintf(stderr, "tup error: Expected is_generated_dir1() to get an SQLite row returned.\n");
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}
	dbrc = sqlite3_column_int(*stmt, 0);
	/* If the exists clause returns 0, then we could be a generated dir,
	 * otherwise we are a normal dir.
	 */
	if(dbrc == 0) {
		rc = 1;
	} else if(dbrc == 1) {
		rc = 0;
	} else {
		fprintf(stderr, "tup error: Expected is_generated_dir1() to get a 0 or 1 from SQLite\n");
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

static int is_generated_dir2(tupid_t dt)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_IS_GENERATED_DIR2];
	static char s[] = "select exists(select 1 from node where dir=? and srcid!=dir and (type=? or type=?))";

	transaction_check("%s [%lli, %i, %i]", s, dt, TUP_NODE_GENERATED, TUP_NODE_GENERATED_DIR);
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
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GENERATED_DIR) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(rc == SQLITE_DONE) {
		fprintf(stderr, "tup error: Expected is_generated_dir2() to get an SQLite row returned.\n");
		goto out_reset;
	}
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		goto out_reset;
	}
	dbrc = sqlite3_column_int(*stmt, 0);
	/* If the exists clause returns 1, then we could be a generated dir,
	 * otherwise we are a normal dir.
	 */
	if(dbrc == 1) {
		rc = 1;
	} else if(dbrc == 0) {
		rc = 0;
	} else {
		fprintf(stderr, "tup error: Expected is_generated_dir2() to get a 0 or 1 from SQLite\n");
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

int tup_db_is_generated_dir(tupid_t dt)
{
	int rc1, rc2;
	/* We are a generated dir if we have no normal files / directories
	 * under us...
	 */
	rc1 = is_generated_dir1(dt);
	if(rc1 < 0)
		return -1;
	if(rc1 == 0)
		return 0;

	/* ... and we have some generated files coming from other dirs or
	 * generated subdirs
	 */
	rc2 = is_generated_dir2(dt);
	if(rc2 < 0)
		return -1;
	if(rc1 == 1 && rc2 == 1)
		return 1;
	return 0;
}

int tup_db_modify_cmds_by_output(tupid_t output, int *modified)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_MODIFY_CMDS_BY_OUTPUT];
	static char s[] = "insert or ignore into modify_list select from_id from normal_link where to_id=?";

	transaction_check("%s [%lli]", s, output);
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
	static char s[] = "insert or ignore into modify_list select to_id from normal_link, node where from_id=? and to_id=id and (type=? or type=?)";

	/* We also flag groups in case we were removed from a group (t3085) */
	transaction_check("%s [%lli, %i, %i]", s, input, TUP_NODE_CMD, TUP_NODE_GROUP);
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
	if(sqlite3_bind_int(*stmt, 3, TUP_NODE_GROUP) != 0) {
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

int tup_db_set_dependent_flags(tupid_t tupid)
{
	/* It's possible this is a file that was included by a Tupfile. Try to
	 * set any dependent directory flags.
	 */
	if(tup_db_set_dependent_dir_flags(tupid) < 0)
		return -1;

	/* It's possible this file is used in a tup.config (eg: tup.config is a
	 * symlink to here).
	 */
	if(tup_db_set_dependent_config_flags(tupid) < 0)
		return -1;

	return 0;
}

int tup_db_set_dependent_dir_flags(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_DEPENDENT_DIR_FLAGS];
	static char s[] = "insert or ignore into create_list select to_id from normal_link, node where from_id=? and to_id=id and type=?";

	transaction_check("%s [%lli, %i]", s, tupid, TUP_NODE_DIR);
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

int tup_db_set_srcid_dir_flags(tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SET_SRCID_DIR_FLAGS];
	static char s[] = "select srcid from node where dir=? and type=?";
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli, %i]", s, tupid, TUP_NODE_GENERATED);
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
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_GENERATED) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

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

		tupid_t newdt = sqlite3_column_int64(*stmt, 0);
		if(newdt != tupid) {
			if(tupid_list_add_tail(&tupid_list, newdt) < 0) {
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

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;

			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(tent->type == TUP_NODE_DIR) {
				if(tup_db_add_create_list(tent->tnode.tupid) < 0)
					return -1;
			}
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

int tup_db_set_dependent_config_flags(tupid_t tupid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_DEPENDENT_CONFIG_FLAGS];
	static char s[] = "insert or ignore into config_list select to_id from normal_link, node where from_id=? and to_id=id and type=?";

	transaction_check("%s [%lli, %i]", s, tupid, TUP_NODE_FILE);
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
	if(sqlite3_bind_int(*stmt, 2, TUP_NODE_FILE) != 0) {
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

static int get_outputs(tupid_t tupid, struct tent_list_head *head)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_OUTPUTS];
	static char s[] = "select to_id from normal_link where from_id=?";
	struct tupid_list_head tupid_list;
	struct tupid_list *tl;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli]", s, tupid);
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

		if(tupid_list_add_tail(&tupid_list, sqlite3_column_int64(*stmt, 0)) < 0) {
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

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;

			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(tent_list_add_tail(head, tent) < 0)
				return -1;
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

int tup_db_select_node_by_link(int (*callback)(void *, struct tup_entry *),
			       void *arg, tupid_t tupid)
{
	struct tent_list_head tent_list;
	struct tent_list *tl;

	tent_list_init(&tent_list);

	if(get_outputs(tupid, &tent_list) < 0)
		return -1;

	tent_list_foreach(tl, &tent_list) {
		if(callback(arg, tl->tent) < 0)
			return -1;
	}
	free_tent_list(&tent_list);

	return 0;
}

int tup_db_select_node_by_sticky_link(int (*callback)(void *, struct tup_entry *),
				      void *arg, tupid_t tupid)
{
	struct tent_entries root = TENT_ENTRIES_INITIALIZER;
	struct tent_tree *tt;

	if(get_sticky_outputs(tupid, &root) < 0)
		return -1;

	RB_FOREACH(tt, tent_entries, &root) {
		if(callback(arg, tt->tent) < 0)
			return -1;
	}
	free_tent_tree(&root);

	return 0;
}

int tup_db_select_node_by_group_link(int (*callback)(void *, struct tup_entry *, struct tup_entry *),
				     void *arg, tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_BY_GROUP_LINK];
	static char s[] = "select to_id, cmdid from group_link where from_id=?";
	struct pair {
		LIST_ENTRY(pair) list;
		tupid_t to_id;
		tupid_t cmdid;
	};
	LIST_HEAD(, pair) pair_list = {NULL};

	transaction_check("%s [%lli]", s, tupid);
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
		struct pair *pair;

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

		pair = malloc(sizeof *pair);
		if(!pair) {
			perror("malloc");
			rc = -1;
			goto out_reset;
		}
		pair->to_id = sqlite3_column_int64(*stmt, 0);
		pair->cmdid = sqlite3_column_int64(*stmt, 1);
		LIST_INSERT_HEAD(&pair_list, pair, list);
	}

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0) {
		while(!LIST_EMPTY(&pair_list)) {
			struct pair *pair = LIST_FIRST(&pair_list);
			struct tup_entry *tent;
			struct tup_entry *cmdtent;

			if(tup_entry_add(pair->to_id, &tent) < 0)
				return -1;
			if(tup_entry_add(pair->cmdid, &cmdtent) < 0)
				return -1;
			if(callback(arg, tent, cmdtent) < 0)
				return -1;
			LIST_REMOVE(pair, list);
			free(pair);
		}
	}

	return rc;
}

int tup_db_select_node_by_distinct_group_link(int (*callback)(void *, struct tup_entry *),
					      void *arg, tupid_t tupid)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_SELECT_NODE_BY_DISTINCT_GROUP_LINK];
	static char s[] = "select distinct(to_id) from group_link where from_id=?";
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli]", s, tupid);
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
		if(tupid_list_add_tail(&tupid_list, sqlite3_column_int64(*stmt, 0)) < 0) {
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

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;

			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(callback(arg, tent) < 0) {
				return -1;
			}
		}
	}
	free_tupid_list(&tupid_list);

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

	transaction_check("%s ['%s', %i]", s, lval, x);
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

	transaction_check("%s ['%s']", s, lval);
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

int tup_db_set_var(tupid_t tupid, const char *value)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_SET_VAR];
	static char s[] = "insert or replace into var values(?, ?)";

	transaction_check("%s [%lli, '%s']", s, tupid, value);
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

static struct var_entry *get_var_id(struct vardb *vdb, struct tup_entry *tent,
				    const char *var, int varlen)
{
	struct var_entry *ve = NULL;
	int dbrc;
	const char *value;
	sqlite3_stmt **stmt = &stmts[_DB_GET_VAR_ID];
	static char s[] = "select value from var where var.id=?";

	transaction_check("%s [%lli]", s, tent->tnode.tupid);
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

	value = (const char *)sqlite3_column_text(*stmt, 0);
	if(!value) {
		goto out_reset;
	}

	ve = vardb_set2(vdb, var, varlen, value, tent);

out_reset:
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return NULL;
	}

	return ve;
}

static struct var_entry *get_var(struct variant *variant, const char *var, int varlen)
{
	struct var_entry *ve;

	ve = vardb_get(&variant->vdb, var, varlen);
	if(!ve) {
		struct tup_entry *tent;

		if(node_select(variant->tent, var, varlen, &tent) < 0)
			return NULL;
		if(!tent) {
			tent = node_insert(variant->tent, var, varlen,
					   NULL, 0, NULL, 0,
					   TUP_NODE_GHOST, INVALID_MTIME, -1);
			if(!tent)
				return NULL;
		}
		if(tent->type == TUP_NODE_VAR) {
			ve = get_var_id(&variant->vdb, tent, var, varlen);
		} else if(varlen == 12 && strncmp(var, "TUP_PLATFORM", varlen) == 0) {
			ve = vardb_set2(&variant->vdb, var, varlen, tup_platform, tent);
		} else if(varlen == 8 && strncmp(var, "TUP_ARCH", varlen) == 0) {
			ve = vardb_set2(&variant->vdb, var, varlen, tup_arch, tent);
		} else {
			ve = vardb_set2(&variant->vdb, var, varlen, "", tent);
		}
	}
	return ve;
}

struct tup_entry *tup_db_get_var(struct variant *variant, const char *var, int varlen, struct estring *e)
{
	struct var_entry *ve;

	ve = get_var(variant, var, varlen);
	if(!ve)
		return NULL;

	if(e) {
		if(estring_append(e, ve->value, ve->vallen) < 0)
			exit(1);
	}
	return ve->tent;
}

static int save_vardict_file(struct vardb *vdb, const char *vardict_file)
{
	int fd;
	int rc = -1;
	unsigned int x;
	struct string_tree *st;

	if(tup_db_var_changed == 0)
		return 0;

	fd = openat(tup_top_fd(), vardict_file, O_CREAT|O_WRONLY|O_TRUNC, 0666);
	if(fd < 0) {
		perror(vardict_file);
		fprintf(stderr, "tup error: Unable to create the vardict file: '%s'\n", vardict_file);
		return -1;
	}
	if(write(fd, &vdb->count, sizeof(vdb->count)) != sizeof(vdb->count)) {
		perror("write");
		fprintf(stderr, "tup error: Unable to write to the vardict file: '%s'\n", vardict_file);
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

static int remove_var_tupid(tupid_t tupid)
{
	tup_db_var_changed++;

	if(var_flag_dirs(tupid) < 0)
		return -1;
	if(tup_db_modify_cmds_by_input(tupid) < 0)
		return -1;
	if(delete_var_entry(tupid) < 0)
		return -1;
	if(delete_name_file(tupid) < 0)
		return -1;
	return 0;
}

static int remove_var(struct var_entry *ve, struct tup_entry *var_dtent)
{
	if(var_dtent) {}

	return remove_var_tupid(ve->tent->tnode.tupid);
}

static int add_var(struct var_entry *ve, struct tup_entry *var_dtent)
{
	struct tup_entry *tent;

	tup_db_var_changed++;

	tent = tup_db_create_node(var_dtent, ve->var.s, TUP_NODE_VAR);
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

int tup_db_read_vars(struct tup_entry *tent, struct tup_entry *vartent, const char *vardict_file)
{
	struct vardb db_tree;
	struct vardb file_tree;
	int dfd;
	int fd;
	int rc;

	vardb_init(&db_tree);
	vardb_init(&file_tree);
	if(tup_db_get_vardb(vartent, &db_tree) < 0)
		return -1;
	dfd = tup_entry_openat(tup_top_fd(), tent->parent);
	if(dfd < 0) {
		rc = 0;
	} else {
		fd = openat(dfd, tent->name.s, O_RDONLY);
		if(fd < 0) {
			if(errno != ENOENT) {
				perror(tent->name.s);
				return -1;
			}
			/* No tup.config == empty file_tree */
			rc = 0;
		} else {
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
	}
	if(rc < 0)
		return -1;

	if(vardb_compare(&db_tree, &file_tree, remove_var, add_var,
			 compare_vars, vartent) < 0)
		return -1;

	if(vardict_file)
		if(save_vardict_file(&file_tree, vardict_file) < 0)
			return -1;

	vardb_close(&file_tree);
	vardb_close(&db_tree);

	return 0;
}

int tup_db_delete_tup_config(struct tup_entry *tent)
{
	struct half_entry_head subdir_list;

	LIST_INIT(&subdir_list);
	if(get_dir_entries(tent->tnode.tupid, &subdir_list) < 0)
		return -1;
	while(!LIST_EMPTY(&subdir_list)) {
		struct half_entry *he = LIST_FIRST(&subdir_list);

		if(remove_var_tupid(he->tupid) < 0)
			return -1;
		LIST_REMOVE(he, list);
		free(he);
	}
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

static int env_cb(int environ_check, struct tup_entry *tent, const char *var, const char *stored_value)
{
	const char *env;
	int envlen = 0;
	int match = 0;
	struct var_entry *ve;
	int varlen;

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
		/* Skip past the 'FOO=' part of the stored value if we have an old value to print */
		const char *old = stored_value ? stored_value + varlen + 1: NULL;
		log_debug_tent("Env update", tent, "\n - Old: %s\n - New: %s\n", old, env);
		printf("Environment variable changed: %s\n", var);
		printf(" - Old: '%s'\n", old);
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
	struct vardb vdb;
	struct string_tree *st;
	struct var_entry *ve;

	if(vardb_init(&vdb) < 0)
		return -1;
	if(tup_db_get_vardb(env_dtent(), &vdb) < 0)
		return -1;
	RB_FOREACH(st, string_entries, &vdb.root) {
		ve = container_of(st, struct var_entry, var);
		if(env_cb(environ_check, ve->tent, ve->var.s, ve->value) < 0)
			return -1;
	}

	if(vardb_close(&vdb) < 0)
		return -1;
	return 0;
}

int tup_db_findenv(const char *var, int varlen, struct var_entry **ret)
{
	struct var_entry *ve;

	if(varlen < 0) {
		varlen = strlen(var);
	}

	ve = vardb_get(&envdb, var, varlen);
	if(!ve) {
		struct tup_entry *newtent;
		const char *newenv;
		int newenvlen = 0;

		newtent = node_insert(env_dtent(), var, varlen, NULL, 0, NULL, 0, TUP_NODE_VAR, INVALID_MTIME, -1);
		if(!newtent)
			return -1;
		char *varname = malloc(varlen + 1);
		if(varname == NULL)
			return -1;
		strncpy(varname, var, varlen);
		varname[varlen] = 0;
		newenv = getenv(varname);
		free(varname);
		if(newenv)
			newenvlen = strlen(newenv);
		ve = envdb_set(var, varlen, newenv, newenvlen, newtent, 1);
		if(!ve)
			return -1;
		expected_changes++;
	}
	*ret = ve;
	return 0;
}

int tup_db_get_environ(struct tent_entries *root,
		       struct tent_entries *normal_root, struct tup_env *te)
{
	struct var_entry *ve;
	struct tent_tree *tt;
	struct tup_entry *tent;
	char ccache_nodirect[] = "CCACHE_NODIRECT=1";
	int ccache_nodirect_len = strlen(ccache_nodirect);
	char *cur;

	te->block_size = 1;
	te->num_entries = 0;
	RB_FOREACH(tt, tent_entries, root) {
		tent = tt->tent;
		if(tent->dt == env_dt()) {
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
				tent_tree_remove(normal_root, tent);
		}
	}
	te->block_size += ccache_nodirect_len + 1;
	te->num_entries++;

	te->envblock = malloc(te->block_size);
	if(!te->envblock) {
		perror("malloc");
		return -1;
	}
	cur = te->envblock;
	RB_FOREACH(tt, tent_entries, root) {
		tent = tt->tent;
		if(tent->dt == env_dt()) {
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
	memcpy(cur, ccache_nodirect, ccache_nodirect_len);
	cur[ccache_nodirect_len] = 0;
	cur += ccache_nodirect_len + 1;

	*cur = 0;
	return 0;
}

static int init_virtual_dirs(void)
{
	struct tup_entry *tent;
	struct tup_entry *root_tent;

	if(tup_entry_add(DOT_DT, &root_tent) < 0)
		return -1;

	tent = tup_db_create_node(root_tent, "$", TUP_NODE_DIR);
	if(!tent) {
		fprintf(stderr, "tup error: Unable to create virtual '$' directory for environment variables.\n");
		return -1;
	}
	local_env_dt = tent->tnode.tupid;

	tent = tup_db_create_node(root_tent, "/", TUP_NODE_DIR);
	if(!tent) {
		fprintf(stderr, "tup error: Unable to create virtual '/' directory for full dependency detection.\n");
		return -1;
	}
	local_slash_dt = tent->tnode.tupid;

	tent = tup_db_create_node(root_tent, "^", TUP_NODE_DIR);
	if(!tent) {
		fprintf(stderr, "tup error: Unable to create virtual '^' directory for environment variables.\n");
		return -1;
	}
	local_exclusion_dt = tent->tnode.tupid;
	return 0;
}

tupid_t env_dt(void)
{
	return local_env_dt;
}

struct tup_entry *env_dtent(void)
{
	static struct tup_entry *tent = NULL;
	if(!tent) {
		if(tup_entry_add(env_dt(), &tent) < 0) {
			fprintf(stderr, "fatal tup error: env_dtent() failed in tup_entry_add()\n");
			exit(1);
		}
	}
	return tent;
}

tupid_t slash_dt(void)
{
	return local_slash_dt;
}

tupid_t exclusion_dt(void)
{
	return local_exclusion_dt;
}

int is_virtual_tent(struct tup_entry *tent)
{
	if(tent->dt == DOT_DT && tent->name.len == 1) {
		if(tent->name.s[0] == '$')
			return 1;
		if(tent->name.s[0] == '/')
			return 1;
		if(tent->name.s[0] == '^')
			return 1;
	}
	return 0;
}

int tup_db_scan_begin(void)
{
	if(tup_db_begin() < 0)
		return -1;
	if(load_existing_nodes() < 0)
		return -1;
	if(variant_load() < 0)
		return -1;
	return 0;
}

int tup_db_scan_end(void)
{
	if(tup_db_commit() < 0)
		return -1;
	return 0;
}

static int load_existing_nodes(void)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_FILES_TO_TREE];
	static char s[] = "select id, dir, type, mtime, mtime_ns, srcid, name, display, flags from node where type=? or type=? or type=? or type=?";

	transaction_check("%s [%i, %i, %i]", s, TUP_NODE_FILE, TUP_NODE_DIR, TUP_NODE_GENERATED, TUP_NODE_GENERATED_DIR);
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
	if(sqlite3_bind_int(*stmt, 4, TUP_NODE_GENERATED_DIR) != 0) {
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
		enum TUP_NODE_TYPE type;
		struct timespec mtime;
		const char *name;
		const char *display;
		const char *flags;
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
		mtime.tv_sec = sqlite3_column_int64(*stmt, 3);
		mtime.tv_nsec = sqlite3_column_int64(*stmt, 4);
		srcid = sqlite3_column_int64(*stmt, 5);
		name = (const char*)sqlite3_column_text(*stmt, 6);
		display = (const char*)sqlite3_column_text(*stmt, 7);
		flags = (const char*)sqlite3_column_text(*stmt, 8);

		if(tup_entry_add_all(tupid, dt, type, mtime, srcid, name, display, flags, NULL) < 0)
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

int tup_db_get_outputs(tupid_t cmdid, struct tent_entries *output_root,
		       struct tent_entries *exclusion_root,
		       struct tup_entry **group)
{
	struct tent_list *tl;
	struct tent_list_head tent_list;
	int rc = 0;

	tent_list_init(&tent_list);

	if(get_outputs(cmdid, &tent_list) < 0)
		return -1;
	if(group)
		*group = NULL;

	tent_list_foreach(tl, &tent_list) {
		struct tup_entry *tent = tl->tent;
		if(tent->type == TUP_NODE_GROUP) {
			if(group) {
				if(*group == NULL) {
					*group = tent;
				} else {
					fprintf(stderr, "tup error: Unable to specify multiple output groups: ");
					print_tup_entry(stderr, *group);
					fprintf(stderr, ", and ");
					print_tup_entry(stderr, tent);
					fprintf(stderr, "\n");
					rc = -1;
					break;
				}
			}
		} else if(exclusion_root && tent->dt == local_exclusion_dt) {
			if(tent_tree_add(exclusion_root, tent) < 0)
				return -1;
		} else {
			if(tent_tree_add(output_root, tent) < 0)
				return -1;
		}
	}
	free_tent_list(&tent_list);

	return rc;
}

static int get_normal_inputs(tupid_t cmdid, struct tent_entries *root, int ghost_check)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_LINKS1];
	static char s[] = "select from_id from normal_link where to_id=?";
	struct tupid_list_head tupid_list;
	struct tupid_list *tl;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli]", s, cmdid);
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
		if(tupid_list_add_tail(&tupid_list, tupid) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;
			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(ghost_check) {
				if(tup_entry_add_ghost_tree(root, tent) < 0)
					return -1;
			} else {
				if(tent_tree_add(root, tent) < 0)
					return -1;
			}
		}
	}
	free_tupid_list(&tupid_list);

	return rc;
}

static int get_sticky_inputs(tupid_t cmdid, struct tent_entries *root,
			     struct tent_entries *group_root)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_LINKS2];
	static char s[] = "select from_id from sticky_link where to_id=?";
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli]", s, cmdid);
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

		if(tupid_list_add_tail(&tupid_list, sqlite3_column_int64(*stmt, 0)) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;
			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(group_root && tent->type == TUP_NODE_GROUP)
				if(tent_tree_add_dup(group_root, tent) < 0)
					return -1;
			if(tent_tree_add_dup(root, tent) < 0)
				return -1;
		}
	}
	free_tupid_list(&tupid_list);
	return rc;
}

static int get_sticky_outputs(tupid_t tupid, struct tent_entries *root)
{
	int rc = 0;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GET_STICKY_OUTPUTS];
	static char s[] = "select to_id from sticky_link where from_id=?";
	struct tupid_list *tl;
	struct tupid_list_head tupid_list;

	tupid_list_init(&tupid_list);

	transaction_check("%s [%lli]", s, tupid);
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

		if(tupid_list_add_tail(&tupid_list, sqlite3_column_int64(*stmt, 0)) < 0) {
			rc = -1;
			break;
		}
	}

	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	if(rc == 0) {
		tupid_list_foreach(tl, &tupid_list) {
			struct tup_entry *tent;
			if(tup_entry_add(tl->tupid, &tent) < 0)
				return -1;
			if(tent_tree_add_dup(root, tent) < 0)
				return -1;
		}
	}
	free_tupid_list(&tupid_list);
	return rc;
}

int tup_db_get_inputs(tupid_t cmdid, struct tent_entries *sticky_root,
		      struct tent_entries *normal_root,
		      struct tent_entries *group_sticky_root)
{
	if(normal_root) {
		if(get_normal_inputs(cmdid, normal_root, 0) < 0)
			return -1;
	}
	if(sticky_root) {
		struct tup_entry *tent;
		if(tup_entry_add(cmdid, &tent) < 0)
			return -1;
		if(tent->retrieved_stickies != sticky_count) {
			free_tent_tree(&tent->stickies);
			free_tent_tree(&tent->group_stickies);
			tent->retrieved_stickies = 0;
		}
		if(!tent->retrieved_stickies) {
			tent->retrieved_stickies = sticky_count;
			if(get_sticky_inputs(cmdid, &tent->stickies, &tent->group_stickies) < 0)
				return -1;
		}
		if(tent_tree_copy(sticky_root, &tent->stickies) < 0) {
			return -1;
		}
		if(group_sticky_root)
			if(tent_tree_copy(group_sticky_root, &tent->group_stickies) < 0) {
				return -1;
			}
	}
	return 0;
}

static int compare_tent_trees(struct tent_entries *a, struct tent_entries *b,
			      void *data,
			      int (*extra_a)(struct tup_entry *tent, void *data),
			      int (*extra_b)(struct tup_entry *tent, void *data))
{
	struct tent_tree *tta;
	struct tent_tree *ttb;

	tta = RB_MIN(tent_entries, a);
	ttb = RB_MIN(tent_entries, b);

	while(tta || ttb) {
		if(!tta) {
			if(extra_b && extra_b(ttb->tent, data) < 0)
				return -1;
			ttb = RB_NEXT(tent_entries, b, ttb);
		} else if(!ttb) {
			if(extra_a && extra_a(tta->tent, data) < 0)
				return -1;
			tta = RB_NEXT(tent_entries, a, tta);
		} else {
			if(tta->tent == ttb->tent) {
				/* Would call same() here if necessary */
				tta = RB_NEXT(tent_entries, a, tta);
				ttb = RB_NEXT(tent_entries, b, ttb);
			} else if(tta->tent->tnode.tupid < ttb->tent->tnode.tupid) {
				if(extra_a && extra_a(tta->tent, data) < 0)
					return -1;
				tta = RB_NEXT(tent_entries, a, tta);
			} else {
				if(extra_b && extra_b(ttb->tent, data) < 0)
					return -1;
				ttb = RB_NEXT(tent_entries, b, ttb);
			}
		}
	}
	return 0;
}

struct actual_output_data {
	FILE *f;
	tupid_t cmdid;
	int output_error;
	struct mapping_head *mapping_list;
	struct tent_entries exclusion_root;
	int do_unlink;
};

static int extra_output(struct tup_entry *tent, void *data)
{
	struct actual_output_data *aod = data;
	struct mapping *map;

	if(!(aod->output_error & 1)) {
		aod->output_error |= 1;
		fprintf(aod->f, "tup error: Unspecified output files - A command is writing to files that you didn't specify in the Tupfile. You should add them so tup knows what to expect.\n");
	}

	if(aod->do_unlink) {
		if(tent->type == TUP_NODE_GHOST) {
			fprintf(aod->f, " -- Delete: ");
			print_tup_entry(aod->f, tent);
			fprintf(aod->f, "\n");
			delete_file(tent);
		} else if(tent->type == TUP_NODE_GENERATED) {
			fprintf(aod->f, " -- Rescheduling command for: ");
			print_tup_entry(aod->f, tent);
			fprintf(aod->f, "\n");
			tup_db_modify_cmds_by_output(tent->tnode.tupid, NULL);
		} else {
			fprintf(aod->f, " -- FILE OVERWRITTEN (you may need to revert this file manually): ");
			print_tup_entry(aod->f, tent);
			fprintf(aod->f, "\n");
		}
	} else {
		fprintf(aod->f, " -- Unspecified output: ");
		print_tup_entry(aod->f, tent);
		fprintf(aod->f, "\n");
	}

	/* The tent could already exist, for example if there is a ghost file
	 * here. In such cases if we didn't actually specify this as an output,
	 * we want to avoid moving the file out of the tmp directory and into
	 * the real fs, so delete the mapping (t4081).
	 */
	TAILQ_FOREACH(map, aod->mapping_list, list) {
		/* Easiest to check for the tent, since the tent is already set
		 * in update_write_info().
		 */
		if(map->tent == tent) {
			del_map(aod->mapping_list, map);
			break;
		}
	}
	/* Return success here so we can display all errant outputs.  Actual
	 * check is in tup_db_check_actual_outputs().
	 */
	return 0;
}

static int missing_output(struct tup_entry *tent, void *data)
{
	struct tup_entry *cmdtent;
	struct actual_output_data *aod = data;

	if(tup_entry_add(aod->cmdid, &cmdtent) < 0)
		return -1;
	fprintf(aod->f, "tup error: Expected to write to file '");
	get_relative_dir(aod->f, NULL, cmdtent->dt, tent->tnode.tupid);
	fprintf(aod->f, "' from cmd %lli but didn't\n", aod->cmdid);

	if(!(aod->output_error & 2)) {
		aod->output_error |= 2;
		/* Return success here so we can display all errant outputs.
		 * Actual check is in tup_db_check_actual_outputs().
		 */
	}
	return 0;
}

int tup_db_check_actual_outputs(FILE *f, tupid_t cmdid,
				struct tent_entries *write_root,
				struct tent_entries *output_root,
				struct mapping_head *mapping_list,
				int *write_bork,
				int do_unlink, int complain_missing)
{
	struct tent_entries output_copy = TENT_ENTRIES_INITIALIZER;
	struct actual_output_data aod = {
		.f = f,
		.cmdid = cmdid,
		.output_error = 0,
		.mapping_list = mapping_list,
		.exclusion_root = TENT_ENTRIES_INITIALIZER,
		.do_unlink = do_unlink,
	};
	int (*missing)(struct tup_entry *, void*) = NULL;

	if(complain_missing)
		missing = missing_output;

	if(tent_tree_copy(&output_copy, output_root) < 0)
		return -1;
	if(compare_tent_trees(write_root, &output_copy, &aod,
			      extra_output, missing) < 0)
		return -1;
	free_tent_tree(&output_copy);
	if(aod.output_error)
		*write_bork = 1;
	return 0;
}

struct write_input_data {
	FILE *f;
	tupid_t cmdid;
	tupid_t groupid;
	int new_groups;
	int normal_links_invalid;
	struct tent_entries *env_root;
	int refactoring;
	int refactoring_failed;
};

static int add_sticky(struct tup_entry *tent, void *data)
{
	struct write_input_data *wid = data;

	if(wid->refactoring) {
		wid->refactoring_failed = 1;
		fprintf(wid->f, "tup refactoring error: Attempting to add a new input link: ");
		print_tup_entry(wid->f, tent);
		fprintf(wid->f, "\n");
		return 0;
	}

	if(tent->type == TUP_NODE_GROUP && wid->groupid != -1) {
		if(group_link_insert(tent->tnode.tupid, wid->groupid, wid->cmdid) < 0)
			return -1;
		wid->new_groups = 1;
	}

	if(tent_tree_search(wid->env_root, tent) != NULL) {
		/* Environment links have to be normal so we can build the
		 * graph properly when they are modified.
		 */
		if(link_insert(tent->tnode.tupid, wid->cmdid, TUP_LINK_NORMAL) < 0)
			return -1;

		/* Also need to make sure we run the command in case this is a
		 * new environment variable.
		 */
		if(tup_db_add_modify_list(wid->cmdid) < 0)
			return -1;
	}
	return link_insert(tent->tnode.tupid, wid->cmdid, TUP_LINK_STICKY);
}

static int rm_sticky(struct tup_entry *tent, void *data)
{
	struct write_input_data *wid = data;
	int exists = 0;

	if(wid->refactoring) {
		wid->refactoring_failed = 1;
		fprintf(wid->f, "tup refactoring error: Attempting to remove an input link: ");
		print_tup_entry(wid->f, tent);
		fprintf(wid->f, "\n");
		return 0;
	}

	if(link_remove(tent->tnode.tupid, wid->cmdid, TUP_LINK_STICKY) < 0)
		return -1;

	if(tup_db_link_exists(tent->tnode.tupid, wid->cmdid, TUP_LINK_NORMAL, &exists) < 0)
		return -1;
	if(exists) {
		 /* Make sure the normal link is removed as well to avoid a circular
		 * dependency (t6045) and environment issues (t4178).
		 */
		if(link_remove(tent->tnode.tupid, wid->cmdid, TUP_LINK_NORMAL) < 0)
			return -1;

		/* Make sure we re-run the command to check for required
		 * inputs.
		 */
		if(tup_db_add_modify_list(wid->cmdid) < 0)
			return -1;
	}

	/* Also check for groups that might need to be removed. */
	if(tent->type == TUP_NODE_GROUP) {
		/* Removing an input group means we need to check if the group
		 * also needs to be removed from the database.
		 */
		if(tup_entry_add_ghost_tree(&ghost_root, tent) < 0)
			return -1;

		/* If this command also writes to a group, we need to remove
		 * the group_link for this input group.
		 */
		if(wid->groupid != -1) {
			if(group_link_remove(tent->tnode.tupid, wid->groupid, wid->cmdid) < 0)
				return -1;
		}

		/* Removing a group input invalidates all file inputs. We can't
		 * just remove all normal links that are in the group, because
		 * the group may have changed since we last used it.
		 */
		wid->normal_links_invalid = 1;
	}
	return 0;
}

int tup_db_write_inputs(FILE *f, tupid_t cmdid, struct tent_entries *input_root,
			struct tent_entries *env_root,
			struct tup_entry *group,
			struct tup_entry *old_group,
			int refactoring)
{
	struct tent_entries sticky_root = TENT_ENTRIES_INITIALIZER;
	struct write_input_data wid = {
		.f = f,
		.cmdid = cmdid,
		.env_root = env_root,
		.groupid = -1,
		.new_groups = 0,
		.normal_links_invalid = 0,
		.refactoring = refactoring,
		.refactoring_failed = 0,
	};

	if(group && group == old_group) {
		wid.groupid = group->tnode.tupid;
	}

	if(tup_db_get_inputs(cmdid, &sticky_root, NULL, NULL) < 0)
		return -1;
	if(compare_tent_trees(input_root, &sticky_root, &wid,
			      add_sticky, rm_sticky) < 0)
		return -1;
	if(wid.normal_links_invalid) {
		struct tent_tree *tt;

		if(tup_db_add_modify_list(cmdid) < 0)
			return -1;
		if(delete_normal_inputs(cmdid) < 0)
			return -1;
		/* Need to re-add the environment links as normal links (t3082) */
		RB_FOREACH(tt, tent_entries, env_root) {
			if(link_insert(tt->tent->tnode.tupid, cmdid, TUP_LINK_NORMAL) < 0)
				return -1;
		}
	}
	free_tent_tree(&sticky_root);

	if(group != old_group) {
		if(old_group) {
			if(delete_group_links(cmdid) < 0)
				return -1;
		}
		if(group) {
			struct tent_tree *tt;

			wid.new_groups = 1;
			/* We now output to a group, so add all of our
			 * group_links.
			 */
			RB_FOREACH(tt, tent_entries, input_root) {
				if(tt->tent->type == TUP_NODE_GROUP) {
					if(group_link_insert(tt->tent->tnode.tupid, group->tnode.tupid, cmdid) < 0)
						return -1;
				}
			}
		}
	}
	if(wid.refactoring_failed)
		return -1;
	if(wid.new_groups) {
		if(add_group_circ_check(group) < 0)
			return -1;
	}
	return 0;
}

struct actual_input_data {
	FILE *f;
	tupid_t cmdid;
	struct variant *cmd_variant;
	struct tent_entries *sticky_root;
	struct tent_entries *output_root;
	struct tent_entries missing_input_root;
	int important_link_removed;
};

static int new_input(struct tup_entry *tent, void *data)
{
	struct actual_input_data *aid = data;
	struct variant *file_variant;

	/* Skip any files that are supposed to be used as outputs */
	if(tent_tree_search(aid->output_root, tent) != NULL)
		return 0;

	file_variant = tup_entry_variant(tent);
	if(!file_variant->root_variant && file_variant != aid->cmd_variant) {
		fprintf(aid->f, "tup error: Unable to use files from another variant (%s) in this variant (%s)\n", file_variant->variant_dir, aid->cmd_variant->variant_dir);
		return -1;
	}

	if(tent->type == TUP_NODE_GENERATED) {
		if(tent_tree_add(&aid->missing_input_root, tent) < 0)
			return -1;
		return 0;
	}
	return 0;
}

static int new_normal_link(struct tup_entry *tent, void *data)
{
	struct actual_input_data *aid = data;

	/* Skip any files that are supposed to be used as outputs */
	if(tent_tree_search(aid->output_root, tent) != NULL)
		return 0;
	/* t6057 - Skip any files that were reported as errors in new_input() */
	if(tent_tree_search(&aid->missing_input_root, tent) != NULL)
		return 0;

	if(tent->type == TUP_NODE_CMD) {
		fprintf(aid->f, "tup error: Attempted to read from a file with the same name as an existing command string. Existing command is: ");
		print_tup_entry(aid->f, tent);
		fprintf(aid->f, "\n");
		return -1;
	}

	return link_insert(tent->tnode.tupid, aid->cmdid, TUP_LINK_NORMAL);
}

static int del_normal_link(struct tup_entry *tent, void *data)
{
	struct actual_input_data *aid = data;

	if(tent->type == TUP_NODE_GENERATED) {
		/* A dependent command may be relying on us for
		 * having this file as a dependency. Make sure they
		 * are not skipped if our outputs are the same.
		 * (t5080).
		 */
		aid->important_link_removed = 1;
	}

	if(link_remove(tent->tnode.tupid, aid->cmdid, TUP_LINK_NORMAL) < 0)
		return -1;
	if(tent_tree_search(aid->sticky_root, tent) == NULL) {
		/* Not a sticky link, so check if it was a ghost (t5054). */
		if(tup_entry_add_ghost_tree(&ghost_root, tent) < 0)
			return -1;
	}
	return 0;
}

static int check_generated_inputs(FILE *f, struct tent_entries *missing_input_root,
				  struct tent_entries *valid_input_root,
				  struct tent_entries *group_root)
{
	int found_error = 0;
	struct tent_tree *tt;
	struct tent_tree *tmp;

	/* First, repeatedly go through the list of missing inputs (ie:
	 * generated files that we read from, but didn't specify in the
	 * Tupfile) to see if we can reach them from a group or from another
	 * generated file that we *did* specify in the Tupfile.
	 */
	RB_FOREACH_SAFE(tt, tent_entries, missing_input_root, tmp) {
		struct tent_tree *grouptt;
		int connected = 0;

		RB_FOREACH(grouptt, tent_entries, group_root) {
			int exists;
			if(tup_db_link_exists(tt->tent->tnode.tupid, grouptt->tent->tnode.tupid, TUP_LINK_NORMAL, &exists) < 0)
				return -1;
			if(exists) {
				connected = 1;
				break;
			}
		}
		if(!connected) {
			if(nodes_are_connected(tt->tent, valid_input_root, &connected) < 0)
				return -1;
		}

		if(connected) {
			tent_tree_rm(missing_input_root, tt);
		}
	}

	/* Anything we couldn't connect is an error. */
	RB_FOREACH(tt, tent_entries, missing_input_root) {
		if(!found_error) {
			fprintf(f, "tup error: Missing input dependency - a file was read from, and was not specified as an input link for the command. This is an issue because the file was created from another command, and without the input link the commands may execute out of order. You should add this file as an input, since it is possible this could randomly break in the future.\n");
			found_error = 1;
		}
		tup_db_print(f, tt->tent->tnode.tupid);
	}
	if(found_error)
		return -1;
	return 0;
}

int tup_db_check_actual_inputs(FILE *f, tupid_t cmdid,
			       struct tent_entries *read_root,
			       struct tent_entries *sticky_root,
			       struct tent_entries *normal_root,
			       struct tent_entries *group_sticky_root,
			       struct tent_entries *output_root,
			       int *important_link_removed)
{
	struct tent_entries sticky_copy = TENT_ENTRIES_INITIALIZER;
	struct actual_input_data aid = {
		.f = f,
		.cmdid = cmdid,
		.sticky_root = sticky_root,
		.output_root = output_root,
		.missing_input_root = TENT_ENTRIES_INITIALIZER,
		.important_link_removed = 0,
	};
	int rc;
	struct tup_entry *cmd_tent;

	if(tup_entry_add(cmdid, &cmd_tent) < 0)
		return -1;
	aid.cmd_variant = tup_entry_variant(cmd_tent);

	if(tent_tree_copy(&sticky_copy, aid.sticky_root) < 0)
		return -1;
	/* First check if we are missing any links that should be sticky. We
	 * don't care about any links that are marked sticky but aren't used.
	 */
	if(compare_tent_trees(read_root, &sticky_copy, &aid,
			      new_input, NULL) < 0)
		return -1;

	rc = check_generated_inputs(f, &aid.missing_input_root, aid.sticky_root, group_sticky_root);

	if(compare_tent_trees(read_root, normal_root, &aid,
			      new_normal_link, del_normal_link) < 0)
		return -1;
	free_tent_tree(&sticky_copy);
	free_tent_tree(&aid.missing_input_root);
	*important_link_removed = aid.important_link_removed;
	return rc;
}

int tup_db_check_config_inputs(struct tup_entry *tent,
			       struct tent_entries *read_root)
{
	struct actual_input_data aid = {
		.f = stdout,
		.cmdid = tent->tnode.tupid,
		.missing_input_root = TENT_ENTRIES_INITIALIZER,
	};
	struct tent_entries output_root = TENT_ENTRIES_INITIALIZER;
	struct tent_entries sticky_root = TENT_ENTRIES_INITIALIZER;
	struct tent_entries normal_root = TENT_ENTRIES_INITIALIZER;

	aid.sticky_root = &sticky_root;
	aid.output_root = &output_root;

	if(tup_db_get_inputs(tent->tnode.tupid, &sticky_root, &normal_root, NULL) < 0)
		return -1;

	if(compare_tent_trees(read_root, &normal_root, &aid,
			      new_normal_link, del_normal_link) < 0)
		return -1;
	free_tent_tree(&normal_root);
	free_tent_tree(&sticky_root);
	return 0;
}

struct parse_output_data {
	FILE *f;
	tupid_t cmdid;
	int outputs_differ;
	struct tup_entry *group;
	int refactoring;
};

static int add_output(struct tup_entry *tent, void *data)
{
	struct parse_output_data *pod = data;

	pod->outputs_differ = 1;

	if(pod->refactoring) {
		fprintf(pod->f, "tup refactoring error: Attempting to add a new output to a command: ");
		print_tup_entry(pod->f, tent);
		fprintf(pod->f, "\n");
		/* Return 0 so we get multiple outputs if there are several.
		 * The actual error return is in the outputs_differ check in
		 * tup_db_write_outputs().
		 */
		return 0;
	}

	if(link_insert(pod->cmdid, tent->tnode.tupid, TUP_LINK_NORMAL) < 0)
		return -1;
	if(pod->group)
		if(link_insert(tent->tnode.tupid, pod->group->tnode.tupid, TUP_LINK_NORMAL) < 0)
			return -1;
	return 0;
}

static int rm_output(struct tup_entry *tent, void *data)
{
	struct parse_output_data *pod = data;

	pod->outputs_differ = 1;

	if(pod->refactoring) {
		fprintf(pod->f, "tup refactoring error: Attempting to remove an output from a command: ");
		print_tup_entry(pod->f, tent);
		fprintf(pod->f, "\n");
		/* Return 0 so we get multiple outputs if there are several.
		 * The actual error return is in the outputs_differ check in
		 * tup_db_write_outputs().
		 */
		return 0;
	}

	if(link_remove(pod->cmdid, tent->tnode.tupid, TUP_LINK_NORMAL) < 0)
		return -1;
	if(pod->group)
		if(link_remove(tent->tnode.tupid, pod->group->tnode.tupid, TUP_LINK_NORMAL) < 0)
			return -1;
	if(tent->type == TUP_NODE_GHOST) {
		/* When an output exclusion is removed, we have to check that
		 * it might now be unused.
		 */
		if(tup_entry_add_ghost_tree(&ghost_root, tent) < 0)
			return -1;
	}
	return 0;
}

int tup_db_write_outputs(FILE *f, struct tup_entry *cmdtent,
			 struct tent_entries *root,
			 struct tent_entries *exclusion_root,
			 struct tup_entry *group,
			 struct tup_entry **old_group,
			 int refactoring, int command_modified)
{
	struct tent_entries output_root = TENT_ENTRIES_INITIALIZER;
	struct tent_entries orig_exclusion_root = TENT_ENTRIES_INITIALIZER;
	tupid_t cmdid = cmdtent->tnode.tupid;
	struct parse_output_data pod = {
		.f = f,
		.cmdid = cmdid,
		.outputs_differ = command_modified,
		.group = NULL,
		.refactoring = refactoring,
	};

	if(tup_db_get_outputs(cmdid, &output_root, &orig_exclusion_root, old_group) < 0)
		return -1;
	if(*old_group == group) {
		/* If we have the same group as before, we just update links to the
		 * group in add/rm_output().
		 */
		pod.group = group;
	} else {
		struct tent_tree *tt;
		pod.outputs_differ = 1;
		if(group) {
			if(refactoring) {
				fprintf(f, "tup refactoring error: Attempting to add a new output group to command %lli: ", cmdid);
				print_tup_entry(f, group);
				fprintf(f, "\n");
				tup_db_print(f, cmdid);
			}
			/* New group - add links from all new outputs */
			RB_FOREACH(tt, tent_entries, root) {
				if(link_insert(tt->tent->tnode.tupid, group->tnode.tupid, TUP_LINK_NORMAL) < 0)
					return -1;
			}
			if(link_insert(cmdid, group->tnode.tupid, TUP_LINK_NORMAL) < 0)
				return -1;
		}
		if(*old_group) {
			if(refactoring) {
				fprintf(f, "tup refactoring error: Attempting to remove the output group from command %lli: ", cmdid);
				print_tup_entry(f, *old_group);
				fprintf(f, "\n");
				tup_db_print(f, cmdid);
			}
			/* Removed from old group - rm links from all old outputs */
			RB_FOREACH(tt, tent_entries, &output_root) {
				if(link_remove(tt->tent->tnode.tupid, (*old_group)->tnode.tupid, TUP_LINK_NORMAL) < 0)
					return -1;

				/* Explicitly add any dependent commands to the
				 * modify_list, so they aren't skipped in case
				 * our outputs are the same (t5078).
				 */
				if(tup_db_modify_cmds_by_input(tt->tent->tnode.tupid) < 0)
					return -1;
			}
			if(link_remove(cmdid, (*old_group)->tnode.tupid, TUP_LINK_NORMAL) < 0)
				return -1;

			/* Any commands that use the group as a resource file
			 * need to be re-executed.
			 */
			if(tup_db_add_modify_list((*old_group)->tnode.tupid) < 0)
				return -1;
			/* Possibly clean up this group if there are no more references. */
			if(tup_entry_add_ghost_tree(&ghost_root, *old_group) < 0)
				return -1;
		}
	}
	if(compare_tent_trees(&output_root, root, &pod, rm_output, add_output) < 0)
		return -1;
	/* Exclusions don't go in groups */
	pod.group = NULL;
	if(compare_tent_trees(&orig_exclusion_root, exclusion_root, &pod, rm_output, add_output) < 0)
		return -1;
	if(pod.outputs_differ == 1) {
		if(refactoring)
			return -1;
		if(tup_db_add_modify_list(cmdid) < 0)
			return -1;

		/* Commands with transient outputs are almost always in the
		 * modify list, since their outputs are generally missing,
		 * which causes the command to go in modify. However, when the
		 * command is changed, we definitely need to re-run it, so we
		 * also put it in the transient list to guarantee that it gets
		 * executed.
		 */
		if(is_transient_tent(cmdtent))
			if(tup_db_add_transient_list(cmdid) < 0)
				return -1;
		if(group) {
			/* Explicitly add the group to the modify list in case
			 * we are skipping outputs (t5079).
			 */
			if(tup_db_add_modify_list(group->tnode.tupid) < 0)
				return -1;
		}
	}
	free_tent_tree(&orig_exclusion_root);
	free_tent_tree(&output_root);
	return 0;
}

struct write_dir_input_data {
	tupid_t dt;
	FILE *f;
};

static int add_dir_link(struct tup_entry *tent, void *data)
{
	struct write_dir_input_data *wdid = data;

	if(tent->type == TUP_NODE_GENERATED) {
		fprintf(wdid->f, "tup error: Unable to read from generated file '");
		print_tup_entry(wdid->f, tent);
		fprintf(wdid->f, "'. Your build configuration must be comprised of files you wrote yourself.\n");
		return -1;
	}
	if(link_insert(tent->tnode.tupid, wdid->dt, TUP_LINK_NORMAL) < 0)
		return -1;
	expected_changes++;
	return 0;
}

static int rm_dir_link(struct tup_entry *tent, void *data)
{
	struct write_dir_input_data *wdid = data;

	if(tup_entry_add_ghost_tree(&ghost_root, tent) < 0)
		return -1;
	if(link_remove(tent->tnode.tupid, wdid->dt, TUP_LINK_NORMAL) < 0)
		return -1;
	expected_changes++;
	return 0;
}

int tup_db_write_dir_inputs(FILE *f, tupid_t dt, struct tent_entries *root)
{
	struct tent_entries sticky_root = TENT_ENTRIES_INITIALIZER;
	struct tent_entries normal_root = TENT_ENTRIES_INITIALIZER;
	struct write_dir_input_data wdid = {
		.dt = dt,
		.f = f,
	};

	if(tup_db_get_inputs(dt, &sticky_root, &normal_root, NULL) < 0)
		return -1;
	if(!RB_EMPTY(&sticky_root)) {
		/* All links to directories should be TUP_LINK_NORMAL */
		fprintf(f, "tup internal error: sticky link found to dir %lli\n", dt);
		return -1;
	}
	if(compare_tent_trees(root, &normal_root, &wdid,
			      add_dir_link, rm_dir_link) < 0)
		return -1;
	free_tent_tree(&sticky_root);
	free_tent_tree(&normal_root);
	return 0;
}

static struct tup_entry *node_insert(struct tup_entry *dtent, const char *name, int namelen,
				     const char *display, int displaylen, const char *flags, int flagslen,
				     enum TUP_NODE_TYPE type, struct timespec mtime, tupid_t srcid)
{
	struct tup_entry *tent;
	if(tup_db_node_insert_tent_display(dtent, name, namelen, display, displaylen, flags, flagslen, type, mtime, srcid, &tent) < 0)
		return NULL;
	if(tent->type == TUP_NODE_DIR && !is_virtual_tent(tent)) {
		if(tup_db_add_create_list(tent->tnode.tupid) < 0)
			return NULL;
	}
	return tent;
}

int tup_db_node_insert_tent(struct tup_entry *dtent, const char *name, int namelen,
			    enum TUP_NODE_TYPE type, struct timespec mtime, tupid_t srcid, struct tup_entry **entry)
{
	return tup_db_node_insert_tent_display(dtent, name, namelen, NULL, 0, NULL, 0, type, mtime, srcid, entry);
}

int tup_db_node_insert_tent_display(struct tup_entry *dtent, const char *name, int namelen,
				    const char *display, int displaylen, const char *flags, int flagslen,
				    enum TUP_NODE_TYPE type, struct timespec mtime, tupid_t srcid, struct tup_entry **entry)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[DB_NODE_INSERT];
	static char s[] = "insert into node(dir, type, name, display, flags, mtime, mtime_ns, srcid) values(?, ?, ?, ?, ?, ?, ?, ?)";
	tupid_t tupid;

	transaction_check("%s [%lli, %i, '%.*s', '%.*s', '%.*s', %li, %li, %lli]", s, dtent->tnode.tupid, type, namelen, name, displaylen, display, flagslen, flags, mtime.tv_sec, mtime.tv_nsec, srcid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dtent->tnode.tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int(*stmt, 2, type) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 3, name, namelen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 4, display, displaylen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_text(*stmt, 5, flags, flagslen, SQLITE_STATIC) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 6, mtime.tv_sec) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 7, mtime.tv_nsec) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 8, srcid) != 0) {
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
	if(sql_debug) {
		fprintf(stderr, " - Inserted node id: %lli\n", tupid);
	}
	if(type == TUP_NODE_CMD) {
		/* New commands go in the modify list so they are executed at
		 * least once.
		 */
		if(tup_db_add_modify_list(tupid) < 0)
			return -1;
	}

	if(tup_entry_add_to_dir(dtent, tupid, name, namelen, display, displaylen, flags, flagslen, type, mtime, srcid, entry) < 0)
		return -1;
	/* It's ok for refactoring to create new nodes, such as for ghosts that
	 * are new inputs (eg: Tuprules.tup or a Tupfile for a new directory.)
	 */
	expected_changes++;

	return 0;
}

static int node_select(struct tup_entry *dtent, const char *name, int len,
		       struct tup_entry **entry)
{
	int rc;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_NODE_SELECT];
	tupid_t tupid;
	enum TUP_NODE_TYPE type;
	struct timespec mtime;
	tupid_t srcid;
	const char *display;
	const char *flags;
	static char s[] = "select id, type, mtime, mtime_ns, srcid, display, flags from node where dir=? and name=?" SQL_NAME_COLLATION;

	*entry = NULL;

	if(tup_entry_find_name_in_dir_dt(dtent, name, len, entry) < 0)
		return -1;
	if(*entry)
		return 0;

	transaction_check("%s [%lli, '%.*s']", s, dtent->tnode.tupid, len, name);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dtent->tnode.tupid) != 0) {
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
	mtime.tv_sec = sqlite3_column_int(*stmt, 2);
	mtime.tv_nsec = sqlite3_column_int(*stmt, 3);
	srcid = sqlite3_column_int64(*stmt, 4);
	display = (const char*)sqlite3_column_text(*stmt, 5);
	flags = (const char*)sqlite3_column_text(*stmt, 6);

	if(tup_entry_add_to_dir(dtent, tupid, name, len, display, -1, flags, -1, type, mtime, srcid, entry) < 0) {
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
	sqlite3_stmt **stmt;
	static char s1[] = "insert into normal_link(from_id, to_id) values(?, ?)";
	static char s2[] = "insert into sticky_link(from_id, to_id) values(?, ?)";
	char *sql;
	int sqlsize;

	if(a == b) {
		fprintf(stderr, "tup error: Attempt made to link a node to itself (%lli)\n", a);
		return -1;
	}
	if(a <= 0 || b <= 0) {
		fprintf(stderr, "tup error: Attmept to insert invalid link: %lli -> %lli\n", a, b);
		return -1;
	}

	switch(style) {
		case TUP_LINK_NORMAL:
			stmt = &stmts[_DB_LINK_INSERT1];
			sql = s1;
			sqlsize = sizeof(s1);
			break;
		case TUP_LINK_STICKY:
			stmt = &stmts[_DB_LINK_INSERT2];
			sql = s2;
			sqlsize = sizeof(s2);
			break;
		default:
			fprintf(stderr, "tup error: Attempt to insert unstyled link %lli -> %lli, style=%i\n", a, b, style);
			return -1;
	}

	transaction_check("%s [%lli, %lli]", sql, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, sql, sqlsize, stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", sql);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	if(style == TUP_LINK_STICKY) {
		struct tup_entry *tent;
		struct tup_entry *srctent;
		if(tup_entry_add(b, &tent) < 0)
			return -1;
		if(tup_entry_add(a, &srctent) < 0)
			return -1;
		if(tent->retrieved_stickies) {
			if(tent_tree_add(&tent->stickies, srctent) < 0)
				return -1;
			if(srctent->type == TUP_NODE_GROUP) {
				if(tent_tree_add(&tent->group_stickies, srctent) < 0)
					return -1;
			}
		}
	}

	return 0;
}

static int link_remove(tupid_t a, tupid_t b, int style)
{
	int rc;
	sqlite3_stmt **stmt;
	static char s1[] = "delete from normal_link where from_id=? and to_id=?";
	static char s2[] = "delete from sticky_link where from_id=? and to_id=?";
	char *sql;
	int sqlsize;

	switch(style) {
		case TUP_LINK_NORMAL:
			stmt = &stmts[_DB_LINK_REMOVE1];
			sql = s1;
			sqlsize = sizeof(s1);
			break;
		case TUP_LINK_STICKY:
			stmt = &stmts[_DB_LINK_REMOVE2];
			sql = s2;
			sqlsize = sizeof(s2);
			break;
		default:
			fprintf(stderr, "tup error: Attempt to remove unstyled link %lli -> %lli, style=%i\n", a, b, style);
			return -1;
	}

	transaction_check("%s [%lli, %lli]", sql, a, b);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, sql, sqlsize, stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", sql);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, a) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}
	if(sqlite3_bind_int64(*stmt, 2, b) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	rc = sqlite3_step(*stmt);
	if(msqlite3_reset(*stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", sql);
		return -1;
	}

	if(style == TUP_LINK_STICKY) {
		struct tup_entry *tent;
		struct tup_entry *srctent;
		if(tup_entry_add(b, &tent) < 0)
			return -1;
		if(tup_entry_add(a, &srctent) < 0)
			return -1;
		if(tent->retrieved_stickies) {
			tent_tree_remove(&tent->stickies, srctent);
			if(srctent->type == TUP_NODE_GROUP) {
				tent_tree_remove(&tent->group_stickies, srctent);
			}
		}
	}

	return 0;
}

static int group_link_insert(tupid_t a, tupid_t b, tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_GROUP_LINK_INSERT];
	static char s[] = "insert into group_link(from_id, to_id, cmdid) values(?, ?, ?)";

	if(a == b) {
		fprintf(stderr, "tup error: Attempt made to group-link a node to itself (%lli)\n", a);
		return -1;
	}
	if(a <= 0 || b <= 0 || cmdid <= 0) {
		fprintf(stderr, "tup error: Attmept to insert invalid group link: %lli -> %lli [%lli]\n", a, b, cmdid);
		return -1;
	}

	transaction_check("%s [%lli, %lli, %lli]", s, a, b, cmdid);
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
	if(sqlite3_bind_int64(*stmt, 3, cmdid) != 0) {
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

static int group_link_remove(tupid_t a, tupid_t b, tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_GROUP_LINK_REMOVE];
	static char s[] = "delete from group_link where from_id=? and to_id=? and cmdid=?";

	transaction_check("%s [%lli, %lli, %lli]", s, a, b, cmdid);
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
	if(sqlite3_bind_int64(*stmt, 3, cmdid) != 0) {
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

static int delete_group_links(tupid_t cmdid)
{
	int rc;
	sqlite3_stmt **stmt = &stmts[_DB_DELETE_GROUP_LINKS];
	static char s[] = "delete from group_link where cmdid=?";

	transaction_check("%s [%lli]", s, cmdid);
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
	static char s[] = "select id from node where dir=? or srcid=?";

	/* This is used to determine if we need to make a real node into a
	 * ghost node. We only need to do that if some other node references it
	 * via dir. We don't care about links because nothing will have a link
	 * to a ghost.
	 */
	transaction_check("%s [%lli, %lli]", s, tupid, tupid);
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

static int add_ghost_checks(tupid_t tupid)
{
	if(get_normal_inputs(tupid, &ghost_root, 1) < 0)
		return -1;
	return 0;
}

static int add_group_and_exclusion_checks(tupid_t tupid)
{
	struct tent_list *tl;
	struct tent_list_head tent_list;

	tent_list_init(&tent_list);

	if(get_outputs(tupid, &tent_list) < 0)
		return -1;

	tent_list_foreach(tl, &tent_list) {
		if(tup_entry_add_ghost_tree(&ghost_root, tl->tent) < 0)
			return -1;
	}
	free_tent_list(&tent_list);

	return 0;
}

void tup_db_del_ghost_tree(struct tup_entry *tent)
{
	tent_tree_remove(&ghost_root, tent);
}

static int reclaim_ghosts(void)
{
	/* All the nodes in ghost_root already are of type TUP_NODE_GHOST,
	 * TUP_NODE_GROUP, or TUP_NODE_GENERATED_DIR. Just make sure they are
	 * no longer needed before deleting them by checking:
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
	struct tent_entries tmp_root = TENT_ENTRIES_INITIALIZER;

	while(!RB_EMPTY(&ghost_root)) {
		struct tup_entry *tent;
		struct tent_tree *tt;
		int rc;

		tt = RB_MIN(tent_entries, &ghost_root);
		tent = tt->tent;
		tent_tree_rm(&ghost_root, tt);
		if(tent->type != TUP_NODE_GHOST && tent->type != TUP_NODE_GROUP &&
		   tent->type != TUP_NODE_GENERATED_DIR) {
			fprintf(stderr, "tup internal error: tup entry %lli in the ghost_root shouldn't be type %i\n", tent->tnode.tupid, tent->type);
			return -1;
		}

		rc = ghost_reclaimable(tent);
		if(rc < 0)
			return -1;
		if(rc == 1 && strcmp(tent->name.s, TUP_CONFIG) != 0) {
			if(sql_debug || reclaim_ghost_debug) {
				fprintf(stderr, "Ghost removed: %lli\n", tent->tnode.tupid);
			}

			/* Don't check us again later, in case we were added
			 * there previously.
			 */
			tent_tree_remove(&tmp_root, tent);

			/* Re-check the parent again later */
			tent_tree_remove(&ghost_root, tent->parent);
			if(tup_entry_add_ghost_tree(&tmp_root, tent->parent) < 0)
				return -1;

			if(rm_generated_dir(tent) < 0)
				return -1;

			if(delete_name_file(tent->tnode.tupid) < 0)
				return -1;
		}

		if(RB_EMPTY(&ghost_root)) {
			/* Swap the current tree with the tmp one where we
			 * stored all the parents to check again.
			 */
			ghost_root.rbh_root = tmp_root.rbh_root;
			tmp_root.rbh_root = NULL;
		}
	}

	return 0;
}

static int ghost_reclaimable(struct tup_entry *tent)
{
	int rc1, rc2;

	if(tent->dt == exclusion_dt()) {
		rc1 = exclusion_reclaimable(tent->tnode.tupid);
		rc2 = rc1;
	} else if(tent->type == TUP_NODE_GHOST || tent->type == TUP_NODE_GENERATED_DIR) {
		rc1 = ghost_reclaimable1(tent->tnode.tupid);
		if(rc1 == 0)
			return 0;
		rc2 = ghost_reclaimable2(tent->tnode.tupid);
	} else {
		rc1 = group_reclaimable1(tent->tnode.tupid);
		if(rc1 == 0)
			return 0;
		rc2 = group_reclaimable2(tent->tnode.tupid);
	}

	/* If either sub-query fails, we fail */
	if(rc1 < 0 || rc2 < 0)
		return -1;
	/* If both checks say it is reclaimable, then it is reclaimable */
	if(rc1 == 1 && rc2 == 1)
		return 1;
	/* Otherwise, it is not reclaimable */
	return 0;
}

static int exclusion_reclaimable(tupid_t tupid)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_EXCLUSION_RECLAIMABLE];
	static char s[] = "select exists(select 1 from normal_link where to_id=?)";

	transaction_check("%s [%lli]", s, tupid);
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
		fprintf(stderr, "tup error: Expected exclusion_reclaimable() to get an SQLite row returned.\n");
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
		fprintf(stderr, "tup error: Expected exclusion_reclaimable() to get a 0 or 1 from SQLite\n");
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

static int group_reclaimable1(tupid_t tupid)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GROUP_RECLAIMABLE1];
	static char s[] = "select exists(select 1 from normal_link where from_id=? or to_id=?)";

	transaction_check("%s [%lli, %lli]", s, tupid, tupid);
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

	if(rc == SQLITE_DONE) {
		fprintf(stderr, "tup error: Expected group_reclaimable1() to get an SQLite row returned.\n");
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
		fprintf(stderr, "tup error: Expected group_reclaimable1() to get a 0 or 1 from SQLite\n");
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

static int group_reclaimable2(tupid_t tupid)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GROUP_RECLAIMABLE2];
	static char s[] = "select exists(select 1 from sticky_link where from_id=?)";

	transaction_check("%s [%lli]", s, tupid);
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
		fprintf(stderr, "tup error: Expected group_reclaimable2() to get an SQLite row returned.\n");
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
		fprintf(stderr, "tup error: Expected group_reclaimable2() to get a 0 or 1 from SQLite\n");
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

static int ghost_reclaimable1(tupid_t tupid)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[_DB_GHOST_RECLAIMABLE1];
	static char s[] = "select exists(select 1 from node where dir=? or srcid=?)";

	transaction_check("%s [%lli, %i]", s, tupid, tupid);
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
	static char s[] = "select exists(select 1 from normal_link where from_id=?)";

	transaction_check("%s [%lli]", s, tupid);
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

int tup_db_reparse_all(void)
{
	char sql_parse_all[] = "insert or replace into create_list select id from node where type=2";
	char *errmsg;

	if(sqlite3_exec(tup_db, sql_parse_all, NULL, NULL, &errmsg) != 0) {
		fprintf(stderr, "SQL error: %s\nQuery was: %s\n",
			errmsg, sql_parse_all);
		return -1;
	}
	return 0;
}

static void print_json(FILE *f, const char *str, int len)
{
	/* Escape \ and " for json strings */
	char *buf = malloc(len * 2);
	const char *s = str;
	int x;
	int offs = 0;

	for(x=0; x<len; x++) {
		if(s[x] == '\\' || s[x] == '"') {
			buf[offs] = '\\';
			offs++;
		}
		buf[offs] = s[x];
		offs++;
	}
	buf[offs] = 0;
	fprintf(f, "%s", buf);
	free(buf);
}

static int print_compile_db(FILE *f, struct tup_entry *cmdtent, struct tup_entry *filetent)
{
	static int first_time = 1;
	struct tup_entry *srctent = variant_tent_to_srctent(cmdtent->parent);
	struct estring e;

	if(first_time) {
		first_time = 0;
	} else {
		fprintf(f, ",\n");
	}
	fprintf(f, "{\n");
	fprintf(f, "    \"directory\": \"");
	estring_init(&e);
	estring_append(&e, get_tup_top(), get_tup_top_len());
	char sep[1] = {path_sep()};
	estring_append(&e, sep, 1);
	if(srctent->tnode.tupid != DOT_DT) {
		if(get_relative_dir_sep(NULL, &e, DOT_DT, srctent->tnode.tupid, path_sep()) < 0)
			return -1;
	}
	print_json(f, e.s, e.len);
	free(e.s);
	fprintf(f, "\",\n");
	fprintf(f, "    \"command\": \"");
	print_json(f, cmdtent->name.s, cmdtent->name.len);
	fprintf(f, "\",\n");
	fprintf(f, "    \"file\": \"");
	if(get_relative_dir(f, NULL, srctent->tnode.tupid, filetent->tnode.tupid) < 0)
		return -1;
	fprintf(f, "\"\n");
	fprintf(f, "}");
	/* Return 1 to indicate we successfully printed an entry */
	return 1;
}

int tup_db_create_compile_db(FILE *f, struct variant *variant)
{
	struct tent_entries root = TENT_ENTRIES_INITIALIZER;
	struct tent_tree *tt;
	int empty = 1;

	if(tup_db_begin() < 0)
		return -1;
	if(tup_db_type_to_tree(&root, TUP_NODE_CMD) < 0)
		return -1;

	fprintf(f, "[\n");
	RB_FOREACH(tt, tent_entries, &root) {
		if(!is_compiledb_tent(tt->tent))
			continue;
		if(tup_entry_variant(tt->tent) != variant)
			continue;

		struct tup_entry *cmdtent = tt->tent;
		struct tent_entries stickies = TENT_ENTRIES_INITIALIZER;
		struct tent_tree *stt;

		if(get_sticky_inputs(cmdtent->tnode.tupid, &stickies, NULL) < 0)
			return -1;

		RB_FOREACH(stt, tent_entries, &stickies) {
			if(stt->tent->type == TUP_NODE_FILE ||
			   stt->tent->type == TUP_NODE_GENERATED) {
				print_compile_db(f, cmdtent, stt->tent);
			}
		}
		free_tent_tree(&stickies);

		empty = 0;
	}
	fprintf(f, "\n]\n");
	if(tup_db_commit() < 0)
		return -1;
	free_tent_tree(&root);
	if(empty) {
		fprintf(stderr, "tup warning: No commands exported to compiledb. You may need to add the ^j flag to commands that should be exported.\n");
	}
	return 0;
}

int tup_db_print_commandline(struct tup_entry *tent)
{
	struct tent_entries root = TENT_ENTRIES_INITIALIZER;
	struct tent_tree *tt;
	int found = 0;

	if(get_sticky_outputs(tent->tnode.tupid, &root) < 0) {
		return -1;
	}

	RB_FOREACH(tt, tent_entries, &root) {
		/* Look through all possible commands that we are a sticky
		 * input to for one that has our name in it. Usually there will
		 * be only one command, but if there are multiple we'd want the
		 * one that lists us on the command-line.
		 */
		if(strstr(tt->tent->name.s, tent->name.s) != NULL) {
			print_compile_db(stdout, tt->tent, tent);
			found = 1;
			break;
		}
	}
	if(!found && root.count > 0) {
		tt = RB_MIN(tent_entries, &root);
		print_compile_db(stdout, tt->tent, tent);
	}

	free_tent_tree(&root);
	return 0;
}

int tup_db_get_vardb(struct tup_entry *dtent, struct vardb *vdb)
{
	int rc = -1;
	int dbrc;
	sqlite3_stmt **stmt = &stmts[DB_GET_VARDB];
	static char s[] = "select node.id, name, value, type from node, var where dir=? and node.id=var.id";

	transaction_check("%s [%i]", s, dtent->tnode.tupid);
	if(!*stmt) {
		if(sqlite3_prepare_v2(tup_db, s, sizeof(s), stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(tup_db));
			fprintf(stderr, "Statement was: %s\n", s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(*stmt, 1, dtent->tnode.tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(tup_db));
		fprintf(stderr, "Statement was: %s\n", s);
		return -1;
	}

	do {
		tupid_t tupid;
		const char *var;
		const char *value;
		enum TUP_NODE_TYPE type;
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
		 * by add_ghost_checks.
		 */
		tent = tup_entry_find(tupid);
		if(!tent)
			if(tup_entry_add_to_dir(dtent, tupid, var, -1, NULL, 0, NULL, 0, type, INVALID_MTIME, -1, &tent) <0)
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
	static char s[] = "insert or ignore into create_list select to_id from normal_link, node where from_id=? and to_id=node.id and node.type=?";

	transaction_check("%s [%lli, %i]", s, tupid, TUP_NODE_DIR);
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

	transaction_check("%s [%lli]", s, tupid);
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
