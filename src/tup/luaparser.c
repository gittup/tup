/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2013  Rendaw <rendaw@zarbosoft.com>
 * Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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
#include "luaparser.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
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
#include <assert.h>

#include "luabuiltin/luabuiltin.h" /* Generated from builtin.lua */
typedef lua_State * scriptdata;

#define SYNTAX_ERROR -2
#define CIRCULAR_DEPENDENCY_ERROR -3

#define parser_error(tf, err_string) fprintf((tf)->f, "%s: %s\n", (err_string), strerror(errno));

#define DISALLOW_NODES 0
#define ALLOW_NODES 1

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

struct tuplua_reader_data {
	struct buf *b;
	int read;
};

struct tuplua_glob_data {
	lua_State *ls;
	const char *directory;
	int directory_size;
	int count;
};

static int include_rules(struct tupfile *tf);
static int include_file(struct tupfile *tf, const char *file);
static int get_path_list(struct tupfile *tf, char *p, struct path_list *plist,
			 tupid_t dt);

static int debug_run = 0;

static const char *tuplua_tostring(struct lua_State *ls, int strindex)
{
	const char *out = luaL_tolstring(ls, strindex, NULL);
	if(out != NULL)
		lua_replace(ls, strindex);
	return out;
}

static const char *tuplua_tolstring(struct lua_State *ls, int strindex, size_t *len)
{
	const char *out = luaL_tolstring(ls, strindex, len);
	if(out != NULL)
		lua_replace(ls, strindex);
	return out;
}

static char *tuplua_strdup(struct lua_State *ls, int strindex)
{
	size_t size;
	const char *source;
	char *out;
	source = tuplua_tolstring(ls, strindex, &size);
	if(!source) return NULL;
	out = malloc(size + 1);
	strncpy(out, source, size);
	out[size] = 0;
	return out;
}

static const char *tuplua_reader(struct lua_State *ls, void *data, size_t *size)
{
	struct tuplua_reader_data *lrd = data;
	if(ls) {}

	if(lrd->read) {
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
	lua_State *oldls = tf->ls;
	char *file = NULL;

	file = tuplua_strdup(ls, -1);
	lua_pop(ls, 1);
	assert(lua_gettop(ls) == 0);
	if(file == NULL)
		return luaL_error(ls, "Must be passed a filename as an argument.");

	tf->ls = ls;
	if(include_file(tf, file) < 0) {
		tf->ls = oldls;
		if (tf->luaerror == TUPLUA_NOERROR) {
			luaL_where(ls, 1);
			lua_pushfstring(ls, "Failed to include file '%s'.", file);
			lua_concat(ls, 2);
			tf->luaerror = TUPLUA_PENDINGERROR;
		}
		free(file);
		return lua_error(ls);
	}
	free(file);
	tf->ls = oldls;

	return 0;
}

static int tuplua_table_to_string(lua_State *ls, const char *table, struct tupfile *tf, char **out)
{
	size_t len = 0;
	char *res;

	lua_getfield(ls, 1, table);
	if(!lua_istable(ls, -1)) {
		lua_pop(ls, 1);
		*out = strdup("");
		return 0;
	}
	lua_pushnil(ls);
	while(lua_next(ls, -2)) {
		size_t tmplen;
		if(tuplua_tolstring(ls, -1, &tmplen) == NULL) {
			fprintf(tf->f, "Element in table '%s' is not a string.  All entries must be path strings.\n", table);
			return -1;
		}

		/* Account for ' ' */
		if(len) len++;
		len += tmplen;
		lua_pop(ls, 1);
	}
	lua_pop(ls, 1);

	res = malloc(len + 1);
	if(!res) {
		perror("malloc");
		return -1;
	}
	*res = '\0';
	*out = res;
	lua_getfield(ls, 1, table);
	lua_pushnil(ls);
	while(lua_next(ls, -2)) {
		size_t tmplen;
		const char *str;

		str = tuplua_tolstring(ls, -1, &tmplen);
		if(!str) {
			fprintf(tf->f, "Element in table '%s' is not a string.  All entries must be path strings.\n", table);
			return -1;
		}

		if(*out != res) {
			*res = ' ';
			res++;
		}
		memcpy(res, str, tmplen);
		res += tmplen;
		*res = '\0';
		lua_pop(ls, 1);
	}
	lua_pop(ls, 1);

	return 0;
}

static int tuplua_function_definerule(lua_State *ls)
{
	struct tupfile *tf = lua_touserdata(ls, lua_upvalueindex(1));
	struct rule r;
	struct bin_head bl;
	size_t command_len = 0;

	LIST_INIT(&bl);

	if(!lua_istable(ls, -1))
		return luaL_error(ls, "This function must be passed a table containing parameters");

	if(tuplua_table_to_string(ls, "inputs", tf, &r.input_pattern) < 0)
		return luaL_error(ls, "Error while parsing 'inputs'.");

	if(tuplua_table_to_string(ls, "outputs", tf, &r.output_pattern) < 0) {
		return luaL_error(ls, "Error while parsing 'outputs'.");
	}

	if(r.input_pattern[0] == 0)
		r.empty_input = 1;
	else
		r.empty_input = 0;

	lua_getfield(ls, 1, "foreach");
	r.foreach = lua_toboolean(ls, -1);

	lua_getfield(ls, 1, "command");
	r.command = tuplua_tolstring(ls, -1, &command_len);
	if(!r.command) {
		return luaL_error(ls, "Parameter 'command' must be a string containing command specification.");
	}
	r.command_len = command_len;

	r.bin = NULL;
	r.line_number = 0;

	if(execute_rule(tf, &r, NULL) < 0)
		return luaL_error(ls, "Failed to define rule.");

	free(r.input_pattern);
	free(r.output_pattern);

	return 0;
}

static int tuplua_function_append_table(lua_State *ls)
{
	int n1 = luaL_len(ls, 1);
	int n2 = luaL_len(ls, 2);
	int x;
	if(!lua_istable(ls, 1))
		return luaL_error(ls, "This function must be passed two tables");
	if(!lua_istable(ls, 2))
		return luaL_error(ls, "This function must be passed two tables");
	for(x=1; x<=n2; x++) {
		lua_rawgeti(ls, 2, x);
		lua_rawseti(ls, 1, n1+x);
	}
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

static int tuplua_function_getdirectory(lua_State *ls)
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
	if(!name)
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
	if(data->directory != NULL) {
		fullpath_length = data->directory_size + 1 + tent->name.len;
		fullpath = malloc(fullpath_length);
		strncpy(fullpath, data->directory, data->directory_size);
		fullpath[data->directory_size] = PATH_SEP;
		strncpy(fullpath + data->directory_size + 1, tent->name.s, tent->name.len);
	} else {
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
	struct path_list_head plist;
	struct path_list pl;
	struct tuplua_glob_data tgd;
	struct tup_entry *srctent = NULL;
	struct tup_entry *dtent;

	TAILQ_INIT(&plist);

	tgd.ls = ls;
	tgd.count = 1; /* Lua numbering starts from 1 */
	tgd.directory = NULL;
	tgd.directory_size = 0;

	lua_settop(ls, 1);

	pattern = tuplua_strdup(ls, -1);
	if(pattern == NULL)
		return luaL_error(ls, "Must be passed a glob pattern as an argument.");
	lua_pop(ls, 1);

	if(get_path_list(tf, pattern, &pl, tf->tupid) < 0) {
		lua_pushfstring(ls, "%s:%d: Failed to parse paths in glob pattern '%s'.", __FILE__, __LINE__, pattern);
		free(pattern);
		return lua_error(ls);
	}

	TAILQ_INSERT_TAIL(&plist, &pl, list);

	if(parse_dependent_tupfiles(&plist, tf) < 0) {
		lua_pushfstring(ls, "%s:%d: Failed to process glob directory for pattern '%s'.", __FILE__, __LINE__, pattern);
		free(pl.pel);
		free(pattern);
		return lua_error(ls);
	}

	if(pl.path != NULL) {
		tgd.directory = pl.path;
		tgd.directory_size = pl.pel->path - pl.path - 1;
	}

	lua_newtable(ls);
	if(tup_entry_add(pl.dt, &dtent) < 0) {
		lua_pushfstring(ls, "%s:%d: Failed to add tup entry when processing glob pattern '%s'.", __FILE__, __LINE__, pattern);
		free(pl.pel);
		free(pattern);
		return lua_error(ls);
	}
	if(dtent->type == TUP_NODE_GHOST) {
		lua_pushfstring(ls, "Unable to generate wildcard for directory '%s' since it is a ghost.\n", pl.path);
		free(pl.pel);
		free(pattern);
		return lua_error(ls);
	}
	if(tup_db_select_node_dir_glob(tuplua_glob_callback, &tgd, pl.dt, pl.pel->path, pl.pel->len, &tf->g->gen_delete_root, 0) < 0) {
		lua_pushfstring(ls, "Failed to glob for pattern '%s' in build(?) tree.", pattern);
		free(pl.pel);
		free(pattern);
		return lua_error(ls);
	}

	if(variant_get_srctent(tf->variant, pl.dt, &srctent) < 0) {
		lua_pushfstring(ls, "Failed to find src tup entry while processing pattern '%s'.", pattern);
		free(pl.pel);
		free(pattern);
		return lua_error(ls);
	}
	if(srctent) {
		if(tup_db_select_node_dir_glob(tuplua_glob_callback, &tgd, srctent->tnode.tupid, pl.pel->path, pl.pel->len, &tf->g->gen_delete_root, 0) < 0) {
			lua_pushfstring(ls, "Failed to glob for pattern '%s' in source(?) tree.", pattern);
			free(pl.pel);
			free(pattern);
			return lua_error(ls);
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
	if(name == NULL)
		return luaL_error(ls, "Must be passed an environment variable name as an argument.");

	if(export(tf, name) < 0)
		return luaL_error(ls, "Failed to export environment variable '%s'.", name);

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
	if(rc < 0 || slen < 0) return 0;

	value = malloc(slen + 1);
	rc = get_relative_dir(value, tf->curtent->tnode.tupid, tid, &slen);
	if(rc < 0 || slen < 0) {
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

	if(tuplua_tolstring(ls, 1, &slen1) == NULL)
		return luaL_error(ls, "Cannot concatenate; Argument 1 cannot be converted to a string.");
	if(tuplua_tolstring(ls, 2, &slen2) == NULL)
		return luaL_error(ls, "Cannot concatenate; Argument 2 cannot be converted to a string.");

	char *out = malloc(slen1 + slen2);
	memcpy(out, tuplua_tostring(ls, 1), slen1);
	memcpy(out + slen1, tuplua_tostring(ls, 2), slen2);
	lua_pushlstring(ls, out, slen1 + slen2);
	free(out);
	return 1;
}

int parse_lua_tupfile(struct tupfile *tf, struct buf *b, const char *name)
{
	struct tuplua_reader_data lrd;
	struct lua_State *ls = NULL;
	int ownstate = 0;

	lrd.read = 0;
	lrd.b = b;

	if(!tf->ls) {
		ownstate = 1;
		ls = luaL_newstate();
		luaL_setoutput(ls, tf->f);
		tf->ls = ls;

		/* Register tup interaction functions in the "tup" table in Lua */
		lua_newtable(ls);
		tuplua_register_function(ls, "include", tuplua_function_include, tf);
		tuplua_register_function(ls, "definerule", tuplua_function_definerule, tf);
		tuplua_register_function(ls, "append_table", tuplua_function_append_table, tf);
		tuplua_register_function(ls, "getcwd", tuplua_function_getcwd, tf);
		tuplua_register_function(ls, "getdirectory", tuplua_function_getdirectory, tf);
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

		/* Load some basic libraries.  File-access functions are
		 * avoided so that accesses must go through the tup methods.
		 * Load the debug library so tracebacks for errors can be
		 * formatted nicely
		 */
		luaL_requiref(ls, "_G", luaopen_base, 1); lua_pop(ls, 1);
		luaL_requiref(ls, LUA_TABLIBNAME, luaopen_table, 1); lua_pop(ls, 1);
		luaL_requiref(ls, LUA_STRLIBNAME, luaopen_string, 1); lua_pop(ls, 1);
		luaL_requiref(ls, LUA_BITLIBNAME, luaopen_bit32, 1); lua_pop(ls, 1);
		luaL_requiref(ls, LUA_MATHLIBNAME, luaopen_math, 1); lua_pop(ls, 1);
		luaL_requiref(ls, LUA_DBLIBNAME, luaopen_debug, 1); lua_pop(ls, 1);
		lua_pushnil(ls); lua_setglobal(ls, "dofile");
		lua_pushnil(ls); lua_setglobal(ls, "loadfile");
		lua_pushnil(ls); lua_setglobal(ls, "load");
		lua_pushnil(ls); lua_setglobal(ls, "require");

		/* Load lua built-in lua helper functions from luabuiltin.h */
		lua_getglobal(ls, "debug");
		lua_getfield(ls, -1, "traceback");
		lua_setfield(ls, LUA_REGISTRYINDEX, "tup_traceback");
		lua_pop(ls, 1);
		lua_getfield(ls, LUA_REGISTRYINDEX, "tup_traceback");
		if(luaL_loadbuffer(ls, (char *)builtin_lua, builtin_lua_len, "builtin") != LUA_OK) {
			fprintf(tf->f, "tup error: Failed to open builtins:\n%s\n", tuplua_tostring(ls, -1));
			lua_close(ls);
			tf->ls = NULL;
			return -1;
		}
		if(lua_pcall(ls, 0, 0, 1) != LUA_OK) {
			fprintf(tf->f, "tup error: Failed to parse builtins:\n%s\n", tuplua_tostring(ls, -1));
			lua_close(ls);
			tf->ls = NULL;
			return -1;
		}
		lua_pop(ls, 1);
	}
	else ls = tf->ls;
	assert(lua_gettop(ls) == 0);

	if(ownstate && (include_rules(tf) < 0)) {
		if(tf->luaerror == TUPLUA_PENDINGERROR) {
			assert(lua_gettop(ls) == 2);
			fprintf(tf->f, "tup error %s\n", tuplua_tostring(ls, -1));
			lua_pop(ls, 1);
			tf->luaerror = TUPLUA_ERRORSHOWN;
		}
		lua_close(ls);
		tf->ls = NULL;
		return -1;
	}
	assert(lua_gettop(ls) == 0);

	lua_getfield(ls, LUA_REGISTRYINDEX, "tup_traceback");

	if(lua_load(ls, &tuplua_reader, &lrd, name, 0) != LUA_OK) {
		if(ownstate) {
			fprintf(tf->f, "tup error %s\n", tuplua_tostring(ls, -1));
			lua_close(ls);
			tf->ls = NULL;
		}
		else {
			tf->luaerror = TUPLUA_PENDINGERROR;
			assert(lua_gettop(ls) == 2);
		}
		return -1;
	}

	if(lua_pcall(ls, 0, 0, 1) != LUA_OK) {
		if(tf->luaerror != TUPLUA_ERRORSHOWN)
			fprintf(tf->f, "tup error %s\n", tuplua_tostring(ls, -1));
		tf->luaerror = TUPLUA_ERRORSHOWN;
		if(ownstate) {
			lua_close(ls);
			tf->ls = NULL;
		} 
		else {
			lua_pop(ls, 2);
			assert(lua_gettop(ls) == 0);
		}
		return -1;
	}

	if(ownstate) {
		lua_close(ls);
		tf->ls = NULL;
	} 
	else {
		lua_pop(ls, 1);
		assert(lua_gettop(ls) == 0);
	}
	return 0;
}

void lua_parser_debug_run(void)
{
	debug_run = 1;
}

static int include_rules(struct tupfile *tf)
{
	char tuprules[] = "Tuprules.lua";
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
		fprintf(tf->f, "tup error: Unable to find directory for include file '%s' relative to '", file);
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
	if(tf->cur_dfd < 0) {
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

	if(parse_lua_tupfile(tf, &incb, file) < 0)
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
