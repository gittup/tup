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

/* _ATFILE_SOURCE needed at least on linux x86_64 */
#define _ATFILE_SOURCE
#include "parser.h"
#include "progress.h"
#include "fileio.h"
#include "fslurp.h"
#include "db.h"
#include "vardb.h"
#include "environ.h"
#include "graph.h"
#include "config.h"
#include "bin.h"
#include "entry.h"
#include "string_tree.h"
#include "container.h"
#include "if_stmt.h"
#include "server.h"
#include "timespan.h"
#include "variant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef lua_State * scriptdata;

#define SYNTAX_ERROR -2
#define CIRCULAR_DEPENDENCY_ERROR -3

#define parser_error(tf, err_string) fprintf((tf)->f, "%s: %s\n", (err_string), strerror(errno));

#define DISALLOW_NODES 0
#define ALLOW_NODES 1

struct name_list_entry {
	TAILQ_ENTRY(name_list_entry) list;
	char *path;
	int len;
	int dirlen;
	struct tup_entry *tent;
};
TAILQ_HEAD(name_list_entry_head, name_list_entry);

struct name_list {
	struct name_list_entry_head entries;
	int num_entries;
	int totlen;
	int basetotlen;
};

struct path_list {
	TAILQ_ENTRY(path_list) list;
	/* For files: */
	char *path;
	struct path_element *pel;
	tupid_t dt;
	/* For bins: */
	struct bin *bin;
};
TAILQ_HEAD(path_list_head, path_list);

struct rule {
	const char *command;
	int command_len;
	struct name_list inputs;
	int empty_input;
	int line_number;
};

struct build_name_list_args {
	struct name_list *nl;
	const char *dir;
	int dirlen;
};

struct tupfile {
	tupid_t tupid;
	struct variant *variant;
	struct tup_entry *curtent;
	struct tup_entry *srctent;
	int cur_dfd;
	int root_fd;
	struct graph *g;
	struct vardb vdb;
	struct tupid_entries cmd_root;
	struct tupid_entries env_root;
	struct tupid_entries input_root;
	FILE *f;
	struct parser_server *ps;
	struct timespan ts;
	char ign;
	char circular_dep_error;
	scriptdata sd;
};

struct tuplua_reader_data
{
	struct buf *b;
	int read;
};

struct tuplua_glob_data
{
	lua_State *ls;
	const char *directory;
	int directory_size;
	int count;
};

static int execute_script(struct buf *b, struct tupfile *tf, const char *name);
static int include_rules(struct tupfile *tf);
static int export(struct tupfile *tf, const char *cmdline);
static int gitignore(struct tupfile *tf);
static int rm_existing_gitignore(struct tupfile *tf, struct tup_entry *tent);
static int include_file(struct tupfile *tf, const char *file);
static int get_path_list(struct tupfile *tf, char *p, struct path_list *plist,
			 tupid_t dt);
static void make_name_list_unique(struct name_list *nl);
static int parse_dependent_tupfiles(struct path_list *plist, struct tupfile *tf);
static int input_nl_add_path(struct tupfile *tf, struct path_list *pl, 
			     struct name_list *nl);
static int output_nl_add_path(struct tupfile *tf, struct path_list *pl, 
			      struct name_list *nl);
static int build_name_list_cb(void *arg, struct tup_entry *tent);
static char *set_path(const char *name, const char *dir, int dirlen);
static int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
		   struct name_list *output_nl);
static void init_name_list(struct name_list *nl);
static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle);
static void delete_name_list(struct name_list *nl);
static void delete_name_list_entry(struct name_list *nl,
				   struct name_list_entry *nle);

static int debug_run = 0;

/* SCRIPTING LANGUAGE CODE START */
static const char *tuplua_tostring(struct lua_State *ls, int strindex)
{
	const char *out = luaL_tolstring(ls, strindex, NULL);
	if (out != NULL)
		lua_replace(ls, strindex);
	return out;
}

static const char *tuplua_tolstring(struct lua_State *ls, int strindex, size_t *len)
{
	const char *out = luaL_tolstring(ls, strindex, len);
	if (out != NULL)
		lua_replace(ls, strindex);
	return out;
}

static char *tuplua_strdup(struct lua_State *ls, int strindex)
{
	size_t size;
	const char *source;
	char *out;
	source = tuplua_tolstring(ls, strindex, &size);
	if (!source) return NULL;
	out = (char *)malloc(size + 1);
	strncpy(out, source, size);
	out[size] = 0;
	return out;
}

static const char *tuplua_reader(struct lua_State *ls, void *data, size_t *size)
{
	struct tuplua_reader_data *lrd = data;
	if(lrd->read)
	{
		*size = 0;
		return 0;
	}

	lrd->read = 1;
	*size = lrd->b->len;
	return lrd->b->s;
}

static void tuplua_register_function(struct lua_State *ls, const char *name, lua_CFunction function, void *data)
{
	lua_pushlightuserdata(ls, data);
	lua_pushcclosure(ls, function, 1);
	lua_setfield(ls, 1, name);
}

static int tuplua_function_include(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	const char *file = NULL;

	file = tuplua_tostring(ls, -1);
	if (file == NULL)
		return luaL_error(ls, "Must be passed a filename as an argument.");
	if(include_file(tf, file) < 0)
		return luaL_error(ls, "Failed to include file \"%s\".", file);
	return 0;
}

static int tuplua_function_includerules(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	if(include_rules(tf) < 0)
		return luaL_error(ls, "Failed to include rules file.");
	return 0;
}

static int tuplua_table_to_namelist(lua_State *ls, const char *table, struct tupfile *tf, struct name_list *nl, int output)
{
	init_name_list(nl);
	lua_getfield(ls, 1, table);
	if(!lua_istable(ls, -1))
	{
		lua_pop(ls, 1);
		return 0;
	}
	lua_pushnil(ls);
	while(lua_next(ls, -2))
	{
		struct path_list pl;
		char *entry = tuplua_strdup(ls, -1);
		if (entry == NULL)
		{
			fprintf(tf->f, "Element in table '%s' is not a string.  All entries must be path strings.\n", table);
			delete_name_list(nl);
			return -1;
		}
		if(get_path_list(tf, entry, &pl, tf->tupid) < -1)
		{
			fprintf(tf->f, "Element '%s' in '%s' is invalid.\n", entry, table);
			free(entry);
			free(pl.pel);
			delete_name_list(nl);
			return -1;
		}
		if ((parse_dependent_tupfiles(&pl, tf) < -1) ||
			(output ? (output_nl_add_path(tf, &pl, nl) < 0) :
				(input_nl_add_path(tf, &pl, nl) < 0)))

		{
			fprintf(tf->f, "Element '%s' in '%s' is invalid.\n", entry, table);
			free(entry);
			free(pl.pel);
			delete_name_list(nl);
			return -1;
		}
		free(entry);
		free(pl.pel);
		lua_pop(ls, 1);
	}
	lua_pop(ls, 1);

	return 0;
}

static int tuplua_function_definerule(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	struct rule r;
	size_t command_len = 0;
	struct name_list output_name_list;

	if(!lua_istable(ls, -1))
		return luaL_error(ls, "This function must be passed a table containing parameters");

	if (tuplua_table_to_namelist(ls, "inputs", tf, &r.inputs, 0) < 0)
		return luaL_error(ls, "Error while parsing 'inputs'.");
	make_name_list_unique(&r.inputs);
	
	if (tuplua_table_to_namelist(ls, "outputs", tf, &output_name_list, 1) < 0)
	{
		delete_name_list(&r.inputs);
		return luaL_error(ls, "Error while parsing 'outputs'.");
	}

	lua_getfield(ls, 1, "command");
	r.command = tuplua_tolstring(ls, -1, &command_len);
	if (!r.command)
	{
		delete_name_list(&r.inputs);
		delete_name_list(&output_name_list);
		return luaL_error(ls, "Parameter \"command\" must be a string containing command specification.");
	}
	if((sizeof(r.command_len) < sizeof(command_len)) &&
		(command_len > ((intmax_t)1 << (sizeof(r.command_len) * 8)) - 1))
	{
		delete_name_list(&r.inputs);
		delete_name_list(&output_name_list);
		return luaL_error(ls, "Parameter \"command\" is too long.");
	}
	r.command_len = command_len;

	r.line_number = 0;

	// Note: This was set to only activate if inputs > 0 or empty_input.
	// I'm not sure how that applies now.
	if(do_rule(tf, &r, &r.inputs, &output_name_list) < 0)
	{
		delete_name_list(&r.inputs);
		delete_name_list(&output_name_list);
		return luaL_error(ls, "Failed to define rule.");
	}
	delete_name_list(&r.inputs);
	delete_name_list(&output_name_list);

	return 0;
}

static int tuplua_function_getcwd(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	int dir_size = 0;
	char *dir = NULL;

	lua_settop(ls, 0);

	if(get_relative_dir(NULL, tf->tupid, tf->curtent->tnode.tupid, &dir_size) < 0) {
		fprintf(tf->f, "tup internal error: Unable to find relative directory length from ID %lli -> %lli\n", tf->tupid, tf->curtent->tnode.tupid);
		tup_db_print(tf->f, tf->tupid);
		tup_db_print(tf->f, tf->curtent->tnode.tupid);
		return luaL_error(ls, "Failed to get directory path length in getcwd.");
	}

	if(dir_size == 0) {
		lua_pushstring(ls, "");
		return 1;
	}

	dir = malloc(dir_size + 1);
	if(get_relative_dir(dir, tf->tupid, tf->curtent->tnode.tupid, &dir_size) < 0) {
		fprintf(tf->f, "tup internal error: Unable to find relative directory length from ID %lli -> %lli\n", tf->tupid, tf->curtent->tnode.tupid);
		tup_db_print(tf->f, tf->tupid);
		tup_db_print(tf->f, tf->curtent->tnode.tupid);
		free(dir);
		return luaL_error(ls, "Failed to get directory path in getcwd.");
	}
	dir[dir_size] = '\0';

	lua_pushlstring(ls, dir, dir_size);
	free(dir);
	return 1;
}

static int tuplua_function_getparent(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));

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
		lua_pushlstring(ls, dirstring, strlen(dirstring));
		return 1;
	} else {
		/* Anywhere else in the hierarchy can just use
		 * the last tup entry as the %d replacement.
		 */
		lua_pushlstring(ls, tf->curtent->name.s, tf->curtent->name.len);
		return 1;
	}
}

static int tuplua_function_getconfig(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	const char *name = NULL;
	size_t name_size = 0;
	char *value = NULL;
	char *value_as_argument = NULL; /* tup_db_get_var moves the pointer, so this is a throwaway */
	int value_size = 0;
	struct tup_entry *tent = NULL;

	name = tuplua_tolstring(ls, -1, &name_size);
	if (!name)
		return luaL_error(ls, "Must be passed an config variable name as an argument.");
	value_size = tup_db_get_varlen(tf->variant, name, name_size) + 1;
	if(value_size < 0)
		luaL_error(ls, "Failed to get config variable length.");
	value = malloc(value_size);
	value_as_argument = value;
	
	tent = tup_db_get_var(tf->variant, name, name_size, &value_as_argument);
	if(!tent)
		return luaL_error(ls, "Failed to get config variable.");
	value[value_size - 1] = '\0';

	if(tupid_tree_add_dup(&tf->input_root, tent->tnode.tupid) < 0)
		return luaL_error(ls, "Failed to get config variable (add_dup).");
	
	lua_pushstring(ls, value);
	free(value);

	return 1;
}

static int tuplua_glob_callback(void *arg, struct tup_entry *tent)
{
	struct tuplua_glob_data *data = arg;
	size_t fullpath_length = 0;
	char *fullpath = NULL;
	if(data->directory != NULL)
	{
		fullpath_length = data->directory_size + 1 + tent->name.len;
		fullpath = malloc(fullpath_length);
		strncpy(fullpath, data->directory, data->directory_size);
		fullpath[data->directory_size] = PATH_SEP;
		strncpy(fullpath + data->directory_size + 1, tent->name.s, tent->name.len);
	}
	else
	{
		fullpath_length = tent->name.len;
		fullpath = tent->name.s;
	}

	lua_pushinteger(data->ls, data->count++);
	lua_pushlstring(data->ls, fullpath, fullpath_length);
	lua_settable(data->ls, -3);

	if(data->directory != NULL)
		free(fullpath);

	return 0;
}

static int tuplua_function_glob(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	char *pattern = NULL;
	struct path_list pl;
	struct tuplua_glob_data tgd;
	struct tup_entry *srctent = NULL;
	struct tup_entry *dtent;

	tgd.ls = ls;
	tgd.count = 1; /* Lua numbering starts from 1 */
	tgd.directory = NULL;
	tgd.directory_size = 0;

	lua_settop(ls, 1);

	pattern = tuplua_strdup(ls, -1);
	if (pattern == NULL)
		return luaL_error(ls, "Must be passed a glob pattern as an argument.");
	lua_pop(ls, 1);

	if(get_path_list(tf, pattern, &pl, tf->tupid) < 0)
	{
		free(pattern);
		return luaL_error(ls, "Failed to parse paths in glob pattern \"%s\".", pattern);
	}

	if(parse_dependent_tupfiles(&pl, tf) < 0)
	{
		free(pl.pel);
		free(pattern);
		return luaL_error(ls, "Failed to process glob directory for pattern \"%s\".", pattern);
	}
	
	if(pl.path != NULL) {
		tgd.directory = pl.path;
		tgd.directory_size = pl.pel->path - pl.path - 1;
	}

	lua_newtable(ls);
	if(tup_entry_add(pl.dt, &dtent) < 0)
	{
		free(pl.pel);
		free(pattern);
		return luaL_error(ls, "Failed to add tup entry when processing glob pattern \"%s\".", pattern);
	}
	if(dtent->type == TUP_NODE_GHOST)
	{
		free(pl.pel);
		free(pattern);
		return luaL_error(ls, "Unable to generate wildcard for directory '%s' since it is a ghost.\n", pl.path);
	}
	if(tup_db_select_node_dir_glob(tuplua_glob_callback, &tgd, pl.dt, pl.pel->path, pl.pel->len, &tf->g->gen_delete_root, 0) < 0)
	{
		free(pl.pel);
		free(pattern);
		return luaL_error(ls, "Failed to glob for pattern \"%s\" in build(?) tree.", pattern);
	}

	if(variant_get_srctent(tf->variant, pl.dt, &srctent) < 0)
	{
		free(pl.pel);
		free(pattern);
		return luaL_error(ls, "Failed to find src tup entry while processing pattern \"%s\".", pattern);
	}
	if(srctent) {
		if(tup_db_select_node_dir_glob(tuplua_glob_callback, &tgd, srctent->tnode.tupid, pl.pel->path, pl.pel->len, &tf->g->gen_delete_root, 0) < 0)
		{
			free(pl.pel);
			free(pattern);
			return luaL_error(ls, "Failed to glob for pattern \"%s\" in source(?) tree.", pattern);
		}
	}
	
	free(pl.pel);
	free(pattern);
	return 1;
}

static int tuplua_function_export(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	const char *name = NULL;

	name = tuplua_tostring(ls, -1);
	if (name == NULL)
		return luaL_error(ls, "Must be passed an environment variable name as an argument.");

	if(export(tf, name) < 0)
		return luaL_error(ls, "Failed to export environment variable \"%s\".", name);

	return 0;
}

static int tuplua_function_creategitignore(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	tf->ign = 1;
	return 0;
}

static int tuplua_function_nodevariable(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	
	lua_settop(ls, 1);

	if(!tuplua_tostring(ls, -1))
		return luaL_error(ls, "Must be passed a string referring to a node as argument 1.");

	struct tup_entry *tent;
	tent = get_tent_dt(tf->curtent->tnode.tupid, tuplua_tostring(ls, 1));
	if(!tent) {
		/* didn't find the given file; if using a variant, check the source dir */
		struct tup_entry *srctent;
		if(variant_get_srctent(tf->variant, tf->curtent->tnode.tupid, &srctent) < 0)
			return luaL_error(ls, "tup error: Internal error locating source tup entry for node variable.");
		if(srctent)
			tent = get_tent_dt(srctent->tnode.tupid, tuplua_tostring(ls, 1));

		if(!tent) {
			return luaL_error(ls, "tup error: Unable to find tup entry for file '%s' in node reference declaration.\n", tuplua_tostring(ls, 1));
		}
	}

	if(tent->type != TUP_NODE_FILE && tent->type != TUP_NODE_DIR) {
		return luaL_error(ls, "tup error: Node-variables can only refer to normal files and directories, not a '%s'.\n", tup_db_type(tent->type));
	}
	
	lua_pop(ls, 1);

	// TODO To guard from users confusing userdata items, allocate extra space and add a type identifier at the beginning (plus a magic number?).
	void *stackid = lua_newuserdata(ls, sizeof(tent->tnode.tupid));
	memcpy(stackid, &tent->tnode.tupid, sizeof(tent->tnode.tupid));
	lua_pushvalue(ls, lua_upvalueindex(2));
	lua_setmetatable(ls, 1);
	
	return 1;
}

static int tuplua_function_nodevariable_tostring(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	
	int slen = 0;
	int rc = -1;

	void *stackid;
	tupid_t tid;
	char *value;
	
	lua_settop(ls, 1);

	if(!lua_isuserdata(ls, 1))
		return luaL_error(ls, "Argument 1 is not a node variable.");
	stackid = lua_touserdata(ls, 1);
	tid = *(tupid_t *)stackid;
	
	rc = get_relative_dir(NULL, tf->curtent->tnode.tupid, tid, &slen);
	if (rc < 0 || slen < 0) return 0;
	
	value = malloc(slen);
	rc = get_relative_dir(value, tf->curtent->tnode.tupid, tid, &slen);
	if (rc < 0 || slen < 0) {
		free(value);
		return 0;
	}

	lua_settop(ls, 0);

	lua_pushlstring(ls, value, slen);
	free(value);

	return 1;
}

static int tuplua_function_concat(struct lua_State *ls)
{
	size_t slen1, slen2;

	if (tuplua_tolstring(ls, 1, &slen1) == NULL)
		return luaL_error(ls, "Cannot concatenate; Argument 1 cannot be converted to a string.");
	if (tuplua_tolstring(ls, 2, &slen2) == NULL)
		return luaL_error(ls, "Cannot concatenate; Argument 2 cannot be converted to a string.");

	char *out = malloc(slen1 + slen2);
	memcpy(out, tuplua_tostring(ls, 1), slen1);
	memcpy(out + slen1, tuplua_tostring(ls, 2), slen2);
	lua_pushlstring(ls, out, slen1 + slen2);
	free(out);
	return 1;
}

static int execute_script(struct buf *b, struct tupfile *tf, const char *name)
{
	struct tuplua_reader_data lrd;
	struct lua_State *ls = NULL;
	int ownstate = 0;
	
	lrd.read = 0;
	lrd.b = b;

	if(!tf->sd)
	{
		ownstate = 1;
		ls = luaL_newstate();
		tf->sd = ls;
	
		/* Register tup interaction functions in the "tup" table in Lua */	
		lua_newtable(ls);
		tuplua_register_function(ls, "dofile", tuplua_function_include, tf);
		tuplua_register_function(ls, "dorulesfile", tuplua_function_includerules, tf);
		tuplua_register_function(ls, "definerule", tuplua_function_definerule, tf);
		tuplua_register_function(ls, "getcwd", tuplua_function_getcwd, tf);
		tuplua_register_function(ls, "getparent", tuplua_function_getparent, tf);
		tuplua_register_function(ls, "getconfig", tuplua_function_getconfig, tf);
		tuplua_register_function(ls, "glob", tuplua_function_glob, tf);
		tuplua_register_function(ls, "export", tuplua_function_export, tf);
		tuplua_register_function(ls, "creategitignore", tuplua_function_creategitignore, tf);
		
		lua_pushlightuserdata(ls, tf);
		lua_newtable(ls);
		lua_pushlightuserdata(ls, tf);
		lua_pushcclosure(ls, tuplua_function_nodevariable_tostring, 1);
		lua_setfield(ls, -2, "__tostring");
		lua_pushlightuserdata(ls, tf);
		lua_pushcclosure(ls, tuplua_function_concat, 1);
		lua_setfield(ls, -2, "__concat");
		lua_pushcclosure(ls, tuplua_function_nodevariable, 2);
		lua_setfield(ls, 1, "nodevariable");
	
		lua_setglobal(ls, "tup");

		/* Load some basic libraries.  File-access functions are avoided so that accesses
		 * must go through the tup methods. Load the debug library so tracebacks 
		 * for errors can be formatted nicely */
		lua_pushcfunction(ls, luaopen_base); lua_pushstring(ls, ""); lua_call(ls, 1, 0);
		lua_pushcfunction(ls, luaopen_table); lua_pushstring(ls, LUA_TABLIBNAME); lua_call(ls, 1, 0);
		lua_pushcfunction(ls, luaopen_string); lua_pushstring(ls, LUA_STRLIBNAME); lua_call(ls, 1, 0);
		lua_pushcfunction(ls, luaopen_bit32); lua_pushstring(ls, LUA_BITLIBNAME); lua_call(ls, 1, 0);
		lua_pushcfunction(ls, luaopen_math); lua_pushstring(ls, LUA_MATHLIBNAME); lua_call(ls, 1, 0);
		lua_pushcfunction(ls, luaopen_debug); lua_pushstring(ls, LUA_DBLIBNAME); lua_call(ls, 1, 0);
		lua_pushnil(ls); lua_setglobal(ls, "dofile");
		lua_pushnil(ls); lua_setglobal(ls, "loadfile");
		lua_pushnil(ls); lua_setglobal(ls, "load");
		lua_pushnil(ls); lua_setglobal(ls, "require");

		luaL_openlibs(ls);
	}
	else ls = tf->sd;

	lua_getglobal(ls, "debug");
	lua_getfield(ls, -1, "traceback");
	lua_remove(ls, -2);

	if(lua_load(ls, &tuplua_reader, &lrd, name, 0) != LUA_OK)
	{
		fprintf(tf->f, "tup error: Failed to open Tupfile:\n%s\n", tuplua_tostring(ls, -1));
		if(ownstate)
		{
			lua_close(ls);
			tf->sd = NULL;
		}
		return 0;
	}

	if(lua_pcall(ls, 0, LUA_MULTRET, 1) != LUA_OK)
	{
		fprintf(tf->f, "tup error: Failed to execute Tupfile:\n%s\n", tuplua_tostring(ls, -1));
		if(ownstate)
		{
			lua_close(ls);
			tf->sd = NULL;
		}
		return 0;
	}

	if(ownstate)
	{
		lua_close(ls);
		tf->sd = NULL;
	}

	return 1;
}
/* SCRIPTING LANGUAGE CODE STOP */

void parser_debug_run(void)
{
	debug_run = 1;
}

int parse(struct node *n, struct graph *g, struct timespan *retts)
{
	struct tupfile tf;
	int fd;
	int rc = -1;
	struct buf b;
	struct parser_server ps;
	struct timeval orig_start;

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
	tf.sd = NULL;

	init_file_info(&ps.s.finfo, tf.variant->variant_dir);
	ps.s.id = n->tnode.tupid;
	pthread_mutex_init(&ps.lock, NULL);

	RB_INIT(&tf.cmd_root);
	RB_INIT(&tf.env_root);
	RB_INIT(&tf.input_root);
	if(server_parser_start(&ps) < 0)
		return -1;

	tf.tupid = n->tnode.tupid;
	tf.curtent = tup_entry_get(tf.tupid);
	tf.root_fd = ps.root_fd;
	tf.g = g;
	if(tf.variant->root_variant) {
		tf.srctent = NULL;
	} else {
		if(variant_get_srctent(tf.variant, tf.tupid, &tf.srctent) < 0)
			goto out_server_stop;
	}
	tf.ign = 0;
	tf.circular_dep_error = 0;
	if(vardb_init(&tf.vdb) < 0)
		goto out_server_stop;
	if(environ_add_defaults(&tf.env_root) < 0)
		goto out_close_vdb;

	/* Keep track of the commands and generated files that we had created
	 * previously. We'll check these against the new ones in order to see
	 * if any should be removed.
	 */
	if(tup_db_dirtype_to_tree(tf.tupid, &g->cmd_delete_root, &g->cmd_delete_count, TUP_NODE_CMD) < 0)
		goto out_close_vdb;
	if(tup_db_dirtype_to_tree(tf.tupid, &g->gen_delete_root, &g->gen_delete_count, TUP_NODE_GENERATED) < 0)
		goto out_close_vdb;

	tf.cur_dfd = tup_entry_openat(ps.root_fd, n->tent);
	if(tf.cur_dfd < 0) {
		fprintf(tf.f, "tup error: Unable to open directory ID %lli\n", tf.tupid);
		goto out_close_vdb;
	}

	fd = openat(tf.cur_dfd, "Tupfile", O_RDONLY);
	if(fd < 0) {
		if(errno == ENOENT) {
			/* No Tupfile means we have nothing to do */
			rc = 0;
			goto out_close_dfd;
		} else {
			parser_error(&tf, "Tupfile");
			goto out_close_dfd;
		}
	}

	if(fslurp_null(fd, &b) < 0)
		goto out_close_file;
	if(!execute_script(&b, &tf, "Tupfile"))
		goto out_free_bs;
	if(tf.ign) {
		if(rm_existing_gitignore(&tf, n->tent) < 0)
			return -1;
		if(gitignore(&tf) < 0) {
			rc = -1;
			goto out_free_bs;
		}
	}
	rc = 0;
out_free_bs:
	free(b.s);
out_close_file:
	if(close(fd) < 0) {
		parser_error(&tf, "close(fd)");
		rc = -1;
	}
out_close_dfd:
	if(close(tf.cur_dfd) < 0) {
		parser_error(&tf, "close(tf.cur_dfd)");
		rc = -1;
	}
out_close_vdb:
	if(vardb_close(&tf.vdb) < 0)
		rc = -1;
out_server_stop:
	if(server_parser_stop(&ps) < 0)
		rc = -1;

	if(rc == 0) {
		if(add_parser_files(tf.f, &ps.s.finfo, &tf.input_root, tf.variant->tent->tnode.tupid) < 0)
			rc = -1;
		if(tup_db_write_dir_inputs(tf.tupid, &tf.input_root) < 0)
			rc = -1;
	}

	pthread_mutex_lock(&ps.lock);
	pthread_mutex_unlock(&ps.lock);
	free_tupid_tree(&tf.env_root);
	free_tupid_tree(&tf.cmd_root);
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
	show_result(n->tent, rc != 0, &tf.ts, NULL);
	if(fflush(tf.f) != 0) {
		/* Use perror, since we're trying to flush the tf.f output */
		perror("fflush");
		rc = -1;
	}
	rewind(tf.f);
	display_output(fileno(tf.f), rc == 0 ? 0 : 3, NULL, 0);
	if(fclose(tf.f) != 0) {
		/* Use perror, since we're trying to close the tf.f output */
		perror("fclose");
		rc = -1;
	}
	if(tf.circular_dep_error)
		rc = CIRCULAR_DEPENDENCY_ERROR;

	return rc;
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

static int export(struct tupfile *tf, const char *cmdline)
{
	struct tup_entry *tent = NULL;

	if(!cmdline[0]) {
		fprintf(tf->f, "tup error: Expected environment variable to export.\n");
		return SYNTAX_ERROR;
	}

	/* Pull from tup's environment */
	if(!tup_db_findenv(cmdline, &tent) < 0) {
		fprintf(tf->f, "tup error: Unable to get tup entry for environment variable '%s'\n", cmdline);
		return -1;
	}
	if(tupid_tree_add_dup(&tf->env_root, tent->tnode.tupid) < 0)
		return -1;
	return 0;
}

static int gitignore(struct tupfile *tf)
{
	char *s;
	int len;
	int fd;

	if(tup_db_alloc_generated_nodelist(&s, &len, tf->tupid, &tf->g->gen_delete_root) < 0)
		return -1;
	if((s && len) || tf->tupid == 1) {
		struct tup_entry *tent;

		if(tup_db_select_tent(tf->tupid, ".gitignore", &tent) < 0)
			return -1;
		if(!tent) {
			if(tup_db_node_insert_tent(tf->tupid, ".gitignore", -1, TUP_NODE_GENERATED, -1, -1, &tent) < 0)
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

		fd = openat(tf->cur_dfd, ".gitignore", O_CREAT|O_WRONLY|O_TRUNC, 0666);
		if(fd < 0) {
			parser_error(tf, ".gitignore");
			fprintf(tf->f, "tup error: Unable to create the .gitignore file.\n");
			return -1;
		}
		if(tf->tupid == 1) {
			if(write(fd, ".tup\n", 5) < 0) {
				parser_error(tf, "write");
				goto err_close;
			}
		}
		if(write(fd, "/.gitignore\n", 12) < 0) {
			parser_error(tf, "write");
			goto err_close;
		}
		if(s && len) {
			if(write(fd, s, len) < 0) {
				parser_error(tf, "write");
				goto err_close;
			}
		}
		if(close(fd) < 0) {
			parser_error(tf, "close(fd)");
			return -1;
		}
	}
	if(s) {
		free(s); /* Freeze gopher! */
	}
	return 0;

err_close:
	close(fd);
	return -1;
}

static int rm_existing_gitignore(struct tupfile *tf, struct tup_entry *tent)
{
	int dfd;
	dfd = tup_entry_open(tent);
	if(dfd < 0)
		return -1;
	if(unlinkat(dfd, ".gitignore", 0) < 0) {
		if(errno != ENOENT) {
			parser_error(tf, "unlinkat");
			fprintf(tf->f, "tup error: Unable to unlink the .gitignore file.\n");
			return -1;
		}
	}
	if(close(dfd) < 0) {
		parser_error(tf, "close(dfd)");
		return -1;
	}
	return 0;
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

	if(get_path_elements(file, &pg) < 0)
		goto out_err;
	if(pg.pg_flags & PG_HIDDEN) {
		fprintf(tf->f, "tup error: Unable to include file with hidden path element.\n");
		goto out_del_pg;
	}
	newdt = find_dir_tupid_dt_pg(tf->f, tf->curtent->tnode.tupid, &pg, &pel, 0, 0);
	if(newdt <= 0) {
		fprintf(tf->f, "tup error: Unable to find directory for include file relative to '");
		print_tup_entry(tf->f, tf->curtent);
		fprintf(tf->f, "'\n");
		goto out_del_pg;
	}
	if(!pel) {
		fprintf(tf->f, "tup error: Invalid include filename: '%s'\n", file);
		goto out_del_pg;
	}

	tf->curtent = tup_entry_get(newdt);

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

	if(!execute_script(&incb, tf, file))
		goto out_free;
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

static int get_path_list(struct tupfile *tf, char *p, struct path_list *pl,
			 tupid_t dt)
{
	pl->path = NULL;
	pl->pel = NULL;
	pl->dt = 0;
	pl->bin = NULL;

	/* Path */
	struct pel_group pg;

	pl->path = p;

	if(get_path_elements(p, &pg) < 0)
		return -1;
	if(pg.pg_flags & PG_HIDDEN) {
		fprintf(tf->f, "tup error: You specified a path '%s' that contains a hidden filename (since it begins with a '.' character). Tup ignores these files - please remove references to it from the Tupfile.\n", p);
		return -1;
	}
	pl->dt = find_dir_tupid_dt_pg(tf->f, dt, &pg, &pl->pel, 0, 0);
	if(pl->dt <= 0) {
		fprintf(tf->f, "tup error: Failed to find directory ID for dir '%s' relative to %lli\n", p, dt);
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

static int parse_dependent_tupfiles(struct path_list *pl, struct tupfile *tf)
{
	/* Only care about non-bins, and directories that are not our
	 * own.
	 */
	if(pl->dt != tf->tupid) {
		struct node *n;

		n = find_node(tf->g, pl->dt);
		if(n != NULL && !n->already_used) {
			int rc;
			struct timespan ts;
			n->already_used = 1;
			rc = parse(n, tf->g, &ts);
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
	return 0;
}

static int input_nl_add_path(struct tupfile *tf, struct path_list *pl, struct name_list *nl)
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
		fprintf(tf->f, "tup error: Explicitly named file '%.*s' is a ghost file, so it can't be used as an input.\n", pl->pel->len, pl->pel->path);
		return -1;
	}
	if(tupid_tree_search(&tf->g->gen_delete_root, tent->tnode.tupid) != NULL) {
		struct tup_entry *srctent = NULL;
		int valid_input = 0;

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
	if(build_name_list_cb(&args, tent) < 0)
		return -1;
	return 0;
}

static int output_nl_add_path(struct tupfile *tf, struct path_list *pl, struct name_list *nl)
{
	if(pl->path || strchr(pl->pel->path, '/')) {
		fprintf(tf->f, "tup error: Attempted to create an output file '%s', which contains a '/' character. Tupfiles should only output files in their own directories.\n - Directory: %lli\n", pl->path, tf->tupid);
		return -1;
	}
	if(name_cmp(pl->pel->path, "Tupfile") == 0 ||
	   name_cmp(pl->pel->path, "Tuprules.tup") == 0 ||
	   name_cmp(pl->pel->path, TUP_CONFIG) == 0) {
		fprintf(tf->f, "tup error: Attempted to generate a file called '%s', which is reserved by tup. Your build configuration must be comprised of files you write yourself.\n", pl->pel->path);
		return -1;
	}
	if(tf->srctent) {
		struct tup_entry *tent;
		if(tup_db_select_tent(tf->srctent->tnode.tupid, pl->pel->path, &tent) < 0)
			return -1;
		if(tent && tent->type != TUP_NODE_GHOST) {
			fprintf(tf->f, "tup error: Attempting to insert '%s' as a generated node when it already exists as a different type (%s) in the source directory. You can do one of two things to fix this:\n  1) If this file is really supposed to be created from the command, delete the file from the filesystem and try again.\n  2) Change your rule in the Tupfile so you aren't trying to overwrite the file.\n", pl->pel->path, tup_db_type(tent->type));
			return -1;
		}
	}

	struct name_list_entry *onle = malloc(sizeof *onle);
	if(!onle) {
		parser_error(tf, "malloc");
		return -1;
	}
	onle->path = strdup(pl->pel->path);
	onle->len = pl->pel->len;
	onle->tent = tup_db_create_node_part(tf->tupid, onle->path, -1,
					     TUP_NODE_GENERATED, -1, NULL);
	if(!onle->tent) {
		free(onle);
		return -1;
	}

	add_name_list_entry(nl, onle);

	return 0;
}

static int build_name_list_cb(void *arg, struct tup_entry *tent)
{
	struct build_name_list_args *args = arg;
	int len;
	struct name_list_entry *nle;

	len = tent->name.len + args->dirlen;

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
	nle->tent = tent;
	nle->dirlen = args->dirlen;

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
				 struct tupid_entries *del_root,
				 tupid_t *cmdid)
{
	struct name_list_entry *onle;
	TAILQ_FOREACH(onle, &onl->entries, list) {
		int rc;
		tupid_t incoming;

		rc = tup_db_get_incoming_link(onle->tent->tnode.tupid, &incoming);
		if(rc < 0)
			return -1;
		/* Only want commands that are still in the del_root. Any
		 * command not in the del_root will mean it has already been
		 * parsed, and so will probably cause an error later in the
		 * duplicate link check.
		 */
		if(incoming != -1) {
			if(tupid_tree_search(del_root, incoming) != NULL) {
				*cmdid = incoming;
				return 0;
			}
		}
	}
	*cmdid = -1;
	return 0;
}

static int do_rule(struct tupfile *tf, struct rule *r, struct name_list *nl,
		   struct name_list *onl)
{
	struct tupid_tree *tt;
	struct tupid_tree *cmd_tt;
	tupid_t cmdid = -1;
	struct name_list_entry *onle = NULL, *nle = NULL;
	struct tupid_entries root = {NULL};
	struct tup_entry *tmptent = NULL;
	struct tup_entry *group = NULL;

	/* If we already have our command string in the db, then use that.
	 * Otherwise, we try to find an existing command of a different
	 * name that points to the output files we are trying to create.
	 * If neither of those cases apply, we just create a new command
	 * node.
	 */
	if(tup_db_select_tent(tf->tupid, r->command, &tmptent) < 0)
		return -1;
	if(tmptent) {
		cmdid = tmptent->tnode.tupid;
		if(tmptent->type != TUP_NODE_CMD) {
			fprintf(tf->f, "tup error: Unable to create command '%s' because the node already exists in the database as type '%s'\n", r->command, tup_db_type(tmptent->type));
			return -1;
		}
	} else {
		if(find_existing_command(onl, &tf->g->cmd_delete_root, &cmdid) < 0)
			return -1;
		if(cmdid == -1) {
			cmdid = create_command_file(tf->tupid, r->command);
		} else {
			if(tup_db_set_name(cmdid, r->command) < 0)
				return -1;
		}
	}

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

	TAILQ_FOREACH(onle, &onl->entries, list) {
		if(tup_db_create_unique_link(tf->f, cmdid, onle->tent->tnode.tupid, &tf->g->cmd_delete_root, &root) < 0) {
			fprintf(tf->f, "tup error: You may have multiple commands trying to create file '%s'\n", onle->path);
			return -1;
		}
		tree_entry_remove(&tf->g->gen_delete_root, onle->tent->tnode.tupid,
				  &tf->g->gen_delete_count);
	}

	if(tup_db_write_outputs(cmdid, &root, group) < 0)
		return -1;
	free_tupid_tree(&root);

	TAILQ_FOREACH(nle, &nl->entries, list) {
		if(tupid_tree_add_dup(&root, nle->tent->tnode.tupid) < 0)
			return -1;
	}
	RB_FOREACH(tt, tupid_entries, &tf->env_root) {
		if(tupid_tree_add_dup(&root, tt->tupid) < 0)
			return -1;
	}
	if(tup_db_write_inputs(cmdid, &root, &tf->env_root, &tf->g->gen_delete_root) < 0)
		return -1;
	free_tupid_tree(&root);
	return 0;
}

static void init_name_list(struct name_list *nl)
{
	TAILQ_INIT(&nl->entries);
	nl->num_entries = 0;
	nl->totlen = 0;
}

static void add_name_list_entry(struct name_list *nl,
				struct name_list_entry *nle)
{
	TAILQ_INSERT_TAIL(&nl->entries, nle, list);
	nl->num_entries++;
	nl->totlen += nle->len;
}

static void delete_name_list(struct name_list *nl)
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

	TAILQ_REMOVE(&nl->entries, nle, list);
	free(nle->path);
	free(nle);
}

