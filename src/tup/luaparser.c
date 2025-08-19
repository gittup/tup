/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2013  Rendaw <rendaw@zarbosoft.com>
 * Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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
#include "estring.h"
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

static struct lua_State *gls;

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

static struct tupfile *top_tupfile(void);
static int get_path_list(struct tupfile *tf, const char *p, struct path_list_head *plist, int orderid);

static struct tupfile_head tupfile_list = SLIST_HEAD_INITIALIZER(tupfile_list);
static int debug_run = 0;

static const char *tuplua_tostring(struct lua_State *ls, int strindex)
{
	const char *out;
	if(lua_isnoneornil(ls, strindex))
		return NULL;
	out = luaL_tolstring(ls, strindex, NULL);
	if(out != NULL)
		lua_replace(ls, strindex);
	return out;
}

static const char *tuplua_tolstring(struct lua_State *ls, int strindex, size_t *len)
{
	const char *out;
	if(lua_isnoneornil(ls, strindex))
		return NULL;
	out = luaL_tolstring(ls, strindex, len);
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

static void tuplua_register_function(struct lua_State *ls, const char *name, lua_CFunction function)
{
	lua_pushcfunction(ls, function);
	lua_setfield(ls, 1, name);
}

static int tuplua_function_include(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	char *file = NULL;

	file = tuplua_strdup(ls, -1);
	lua_pop(ls, 1);
	assert(lua_gettop(ls) == 0);
	if(file == NULL)
		return luaL_error(ls, "Must be passed a filename as an argument.");

	if(parser_include_file(tf, file) < 0) {
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

	return 0;
}

static int tuplua_table_to_path_list(lua_State *ls, const char *table, struct tupfile *tf, struct path_list_head *plist, int allow_nodes)
{
	int orderid = 1;
	lua_getfield(ls, 1, table);
	if(!lua_istable(ls, -1)) {
		lua_pop(ls, 1);
		return 0;
	}

	lua_pushnil(ls);
	while(lua_next(ls, -2)) {
		const char *path;
		char *evalp;

		path = tuplua_tostring(ls, -1);
		if(!path)
			return luaL_error(ls, "tuplua_table_to_path_list() called with a nil path");
		evalp = eval(tf, path, allow_nodes);
		if(!evalp)
			return luaL_error(ls, "tuplua_table_to_path_list() failed to evaluate string");
		if(get_path_list(tf, evalp, plist, orderid) < 0)
			return luaL_error(ls, "tuplua_table_to_path_list() failed in get_path_list()");
		free(evalp);
		lua_pop(ls, 1);
		orderid++;
	}

	return 0;
}

static char *tuplua_table_tostring(lua_State *ls)
{
	struct estring e;
	int first = 1;

	if(estring_init(&e) < 0)
		return NULL;
	lua_pushnil(ls);
	while(lua_next(ls, -2)) {
		const char *path;

		path = tuplua_tostring(ls, -1);
		if(!path)
			return NULL;
		if(!first) {
			if(estring_append(&e, " ", 1) < 0)
				return NULL;
		}
		first = 0;
		if(estring_append(&e, path, strlen(path)) < 0)
			return NULL;
		lua_pop(ls, 1);
	}

	return e.s;
}

static int tuplua_function_definerule(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	struct rule r;
	struct path_list_head input_path_list;
	struct name_list return_nl;
	struct name_list_entry *nle;
	size_t command_len = 0;
	const char *bin;
	int is_variant_copy = 0;
	int count = 1;

	init_rule(&r);
	init_name_list(&return_nl);

	if(!lua_istable(ls, -1))
		return luaL_error(ls, "This function must be passed a table containing parameters");

	TAILQ_INIT(&input_path_list);
	if(tuplua_table_to_path_list(ls, "inputs", tf, &input_path_list, EXPAND_NODES_SRC) < 0)
		return luaL_error(ls, "Error while parsing 'inputs'.");
	if(tuplua_table_to_path_list(ls, "extra_inputs", tf, &r.order_only_input_paths, EXPAND_NODES_SRC) < 0)
		return luaL_error(ls, "Error while parsing 'extra_inputs'.");
	if(tuplua_table_to_path_list(ls, "outputs", tf, &r.outputs, EXPAND_NODES_SRC) < 0)
		return luaL_error(ls, "Error while parsing 'outputs'.");
	if(tuplua_table_to_path_list(ls, "extra_outputs", tf, &r.extra_outputs, EXPAND_NODES_SRC) < 0)
		return luaL_error(ls, "Error while parsing 'extra_outputs'.");

	lua_getfield(ls, 1, "bin");
	bin = tuplua_tolstring(ls, -1, NULL);
	if(bin) {
		r.bin = bin_add(bin, &tf->bin_list);
	}

	lua_getfield(ls, 1, "command");
	r.command = tuplua_tolstring(ls, -1, &command_len);
	if(!r.command) {
		return luaL_error(ls, "Parameter 'command' must be a string containing command specification.");
	}
	r.command_len = command_len;
	if(strcmp(r.command, "!tup_preserve") == 0)
		is_variant_copy = 1;

	if(parse_dependent_tupfiles(&input_path_list, tf) < 0)
		return luaL_error(ls, "Error while parsing dependent Tupfiles");
	if(get_name_list(tf, &input_path_list, &r.inputs, is_variant_copy) < 0)
		return luaL_error(ls, "Error parsing input list");

	if(TAILQ_EMPTY(&input_path_list))
		r.empty_input = 1;

	lua_getfield(ls, 1, "foreach");
	r.foreach = lua_toboolean(ls, -1);

	if(execute_rule(tf, &r, &return_nl) < 0)
		return luaL_error(ls, "Failed to execute rule.");

	free_path_list(&input_path_list);

	lua_newtable(ls);
	TAILQ_FOREACH(nle, &return_nl.entries, list) {
		struct estring e;
		estring_init(&e);
		if(get_relative_dir(NULL, &e, tf->tent->tnode.tupid, nle->tent->tnode.tupid) < 0)
			return luaL_error(ls, "Unable to get relative path of output file.");
		lua_pushinteger(ls, count);
		lua_pushlstring(ls, e.s, e.len);
		lua_settable(ls, -3);
		count++;
		free(e.s);
	}
	free_path_list(&r.order_only_input_paths);
	free_path_list(&r.outputs);
	free_path_list(&r.extra_outputs);
	delete_name_list(&return_nl);

	return 1;
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
	struct tupfile *tf = top_tupfile();
	struct estring e;

	lua_settop(ls, 0);

	if(estring_init(&e) < 0)
		return luaL_error(ls, "Error allocating memory in tuplua_function_getcwd()");

	if(get_relative_dir(NULL, &e, tf->tent->tnode.tupid, tf->curtent->tnode.tupid) < 0) {
		fprintf(tf->f, "tup internal error: Unable to find relative directory from ID %lli -> %lli\n", tf->tent->tnode.tupid, tf->curtent->tnode.tupid);
		tup_db_print(tf->f, tf->tent->tnode.tupid);
		tup_db_print(tf->f, tf->curtent->tnode.tupid);
		return luaL_error(ls, "Failed to get relative directory in getcwd.");
	}

	lua_pushlstring(ls, e.s, e.len);
	free(e.s);
	return 1;
}

static int tuplua_function_getvariantdir(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();

	lua_settop(ls, 0);

	char value[32];
	snprintf(value, 31, "%%%llit", tf->curtent->tnode.tupid);
	value[31] = 0;
	lua_pushlstring(ls, value, strlen(value));
	return 1;
}

static int tuplua_function_getvariantoutputdir(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	struct estring e;

	lua_settop(ls, 0);

	estring_init(&e);
	if(get_relative_dir(NULL, &e, tf->srctent->tnode.tupid, tf->tent->tnode.tupid) < 0) {
		fprintf(tf->f, "tup internal error: Unable to find relative directory from ID %lli -> %lli\n", tf->srctent->tnode.tupid, tf->tent->tnode.tupid);
		tup_db_print(tf->f, tf->srctent->tnode.tupid);
		tup_db_print(tf->f, tf->tent->tnode.tupid);
		return luaL_error(ls, "Failed to get directory path length in getcwd.");
	}

	lua_pushlstring(ls, e.s, e.len);
	free(e.s);
	return 1;
}

static int tuplua_function_getdirectory(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();

	if(tf->tent->tnode.tupid == DOT_DT) {
		/* At the top of the tup-hierarchy, we get the
		 * directory from where .tup is stored, since
		 * the top-level tup entry is just "."
		 */
		char *last_slash;
		const char *dirstring;

		last_slash = strrchr(get_tup_top(), path_sep());
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

static int tuplua_function_getrelativedir(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	const char *dirname;
	tupid_t dest;
	struct estring e;

	if(estring_init(&e) < 0)
		return luaL_error(ls, "tup.getrelativedir() failed to initialize an estring");

	dirname = tuplua_tostring(ls, -1);
	if(!dirname)
		return luaL_error(ls, "tup.getrelativedir() called with a nil path");
	dest = find_dir_tupid_dt(tf->tent->tnode.tupid, dirname, NULL, 0, 0);
	if(dest < 0)
		return luaL_error(ls, "Failed to find tup entry for '%s' relative to the current Tupfile", dirname);
	if(get_relative_dir(NULL, &e, dest, tf->tent->tnode.tupid) < 0)
		return luaL_error(ls, "tup.getrelativedir() failed to get relative path from tupid %lli to %lli", dest, tf->tent->tnode.tupid);
	lua_pushlstring(ls, e.s, e.len);
	free(e.s);
	return 1;
}

static int tuplua_function_getconfig(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	const char *name = NULL;
	size_t name_size = 0;
	struct tup_entry *tent = NULL;
	struct estring e;

	if(estring_init(&e) < 0)
		return luaL_error(ls, "Error allocating memory in tuplua_function_getconfig()");

	name = tuplua_tolstring(ls, -1, &name_size);
	if(!name)
		return luaL_error(ls, "Must be passed an config variable name as an argument.");

	tent = tup_db_get_var(tf->variant, name, name_size, &e);
	if(!tent)
		return luaL_error(ls, "Failed to get config variable.");

	if(tent_tree_add_dup(&tf->input_root, tent) < 0)
		return luaL_error(ls, "Failed to get config variable (add_dup).");

	lua_pushstring(ls, e.s);
	free(e.s);

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
		fullpath[data->directory_size] = path_sep();
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
	struct tupfile *tf = top_tupfile();
	const char *pattern = NULL;
	struct path_list_head plist;
	struct path_list *pl;
	struct tuplua_glob_data tgd;
	struct tup_entry *srctent = NULL;
	struct tup_entry *dtent;

	TAILQ_INIT(&plist);

	tgd.ls = ls;
	tgd.count = 1; /* Lua numbering starts from 1 */
	tgd.directory = NULL;
	tgd.directory_size = 0;

	lua_settop(ls, 1);

	pattern = tuplua_tostring(ls, -1);
	if(pattern == NULL)
		return luaL_error(ls, "Must be passed a glob pattern as an argument.");
	lua_pop(ls, 1);

	if(get_path_list(tf, pattern, &plist, 1) < 0) {
		lua_pushfstring(ls, "%s:%d: Failed to parse paths in glob pattern '%s'.", __FILE__, __LINE__, pattern);
		return lua_error(ls);
	}

	pl = TAILQ_FIRST(&plist);

	if(parse_dependent_tupfiles(&plist, tf) < 0) {
		lua_pushfstring(ls, "%s:%d: Failed to process glob directory for pattern '%s'.", __FILE__, __LINE__, pattern);
		free_path_list(&plist);
		return lua_error(ls);
	}

	if(pl->dir != NULL) {
		tgd.directory = pl->dir;
		tgd.directory_size = pl->dirlen;
	}

	lua_newtable(ls);
	if(tup_entry_add(pl->dt, &dtent) < 0) {
		lua_pushfstring(ls, "%s:%d: Failed to add tup entry when processing glob pattern '%s'.", __FILE__, __LINE__, pattern);
		free_path_list(&plist);
		return lua_error(ls);
	}
	if(dtent->type == TUP_NODE_GHOST) {
		lua_pushfstring(ls, "Unable to generate wildcard for directory '%s' since it is a ghost.\n", pl->mem);
		free_pel(pl->pel);
		return lua_error(ls);
	}
	if(tup_db_select_node_dir_glob(tuplua_glob_callback, &tgd, dtent, pl->pel->path, pl->pel->len, &tf->g->gen_delete_root, 0) < 0) {
		lua_pushfstring(ls, "Failed to glob for pattern '%s' in build(?) tree.", pattern);
		free_path_list(&plist);
		return lua_error(ls);
	}

	if(variant_get_srctent(tf->variant, dtent, &srctent) < 0) {
		lua_pushfstring(ls, "Failed to find src tup entry while processing pattern '%s'.", pattern);
		free_pel(pl->pel);
		return lua_error(ls);
	}
	if(srctent) {
		if(tup_db_select_node_dir_glob(tuplua_glob_callback, &tgd, srctent, pl->pel->path, pl->pel->len, &tf->g->gen_delete_root, 0) < 0) {
			lua_pushfstring(ls, "Failed to glob for pattern '%s' in source(?) tree.", pattern);
			free_path_list(&plist);
			return lua_error(ls);
		}
	}

	free_path_list(&plist);
	return 1;
}

static int tuplua_function_export(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	const char *name = NULL;

	name = tuplua_tostring(ls, -1);
	if(name == NULL)
		return luaL_error(ls, "Must be passed an environment variable name as an argument.");

	if(export(tf, name) < 0)
		return luaL_error(ls, "Failed to export environment variable '%s'.", name);

	return 0;
}

static int tuplua_function_import(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	const char *name = NULL;
	const char *var = NULL;
	const char *val = NULL;

	name = tuplua_tostring(ls, -1);
	if(name == NULL)
		return luaL_error(ls, "Must be passed an environment variable name as an argument.");

	if(import(tf, name, &var, &val) < 0)
		return luaL_error(ls, "Failed to import environment variable '%s'.", name);
	if(val) {
		lua_pushstring(ls, val);
	} else {
		lua_pushnil(ls);
	}
	lua_setglobal(ls, var);
	return 0;
}

static int tuplua_function_creategitignore(lua_State *ls)
{
	if(ls) {/* unused */}
	struct tupfile *tf = top_tupfile();
	tf->ign = 1;
	return 0;
}

static int tuplua_function_handle_fileread(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	const char *filename;
	const char *mode;

	filename = tuplua_tostring(ls, 1);
	if(!filename)
		return luaL_error(ls, "chdir() must be passed a filename for Windows dependencies");
	mode = tuplua_tostring(ls, 2);
	if(!mode) {
		mode = "r";
	}
	if(strcmp(mode, "r") != 0) {
		return luaL_error(ls, "io.open in the parser can only open files read-only");
	}
	if(is_full_path(filename)) {
		if(handle_file(ACCESS_READ, filename, "", &tf->ps->s.finfo) < 0)
			return luaL_error(ls, "unable to save file read access in the lua parser");
	} else {
		char fullpath[PATH_MAX];
		char curtentpath[PATH_MAX];
		if(snprint_tup_entry(curtentpath, sizeof(curtentpath), tf->curtent) >= (int)sizeof(curtentpath)) {
			return luaL_error(ls, "string size too small in handle_fileread()\n");
		}
		if(snprintf(fullpath, PATH_MAX, "%s/%s/%s", get_tup_top(), curtentpath, filename) >= PATH_MAX) {
			return luaL_error(ls, "string size too small in handle_fileread()\n");
		}
		if(handle_file(ACCESS_READ, fullpath, "", &tf->ps->s.finfo) < 0)
			return luaL_error(ls, "unable to save file read access in the lua parser");
	}

	/* builtin.lua will call unchdir after doing the actual file open,
	 * which is expected to be relative to the current directory.
	 */
	if(fchdir(tf->cur_dfd) < 0) {
		perror("fchdir");
		return luaL_error(ls, "tup error: Unable to chdir into virtual tup directory for io.open");
	}
	return 0;
}

static int tuplua_function_unchdir(lua_State *ls)
{
	if(fchdir(tup_top_fd()) < 0) {
		perror("fchdir");
		return luaL_error(ls, "tup error: Unable to chdir back to root tup directory for io.open");
	}
	return 0;
}

static int tuplua_function_run(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	const char *cmdline;

	cmdline = tuplua_tostring(ls, 1);
	if(!cmdline)
		return luaL_error(ls, "run() must be passed a string for the command-line to run");

	if(exec_run_script(tf, cmdline, 0) < 0)
		return luaL_error(ls, "tup error: Failed to run external script.\n");

	return 0;
}

static int tuplua_function_nodevariable(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();

	lua_settop(ls, 1);

	if(!tuplua_tostring(ls, -1))
		return luaL_error(ls, "Must be passed a string referring to a node as argument 1.");

	struct tup_entry *tent;
	tent = get_tent_dt(tf->curtent->tnode.tupid, tuplua_tostring(ls, 1));
	if(!tent) {
		/* didn't find the given file; if using a variant, check the source dir */
		struct tup_entry *srctent;
		if(variant_get_srctent(tf->variant, tf->curtent, &srctent) < 0)
			return luaL_error(ls, "tup error: Internal error locating source tup entry for node variable.");
		if(srctent)
			tent = get_tent_dt(srctent->tnode.tupid, tuplua_tostring(ls, 1));

		if(!tent) {
			return luaL_error(ls, "tup error: Unable to find tup entry for file '%s' in node reference declaration.", tuplua_tostring(ls, 1));
		}
	}

	if(tent->type != TUP_NODE_FILE && tent->type != TUP_NODE_DIR) {
		return luaL_error(ls, "tup error: Node-variables can only refer to normal files and directories, not a '%s'.", tup_db_type(tent->type));
	}

	lua_pop(ls, 1);

	// TODO To guard from users confusing userdata items, allocate extra space and add a type identifier at the beginning (plus a magic number?).
	void *stackid = lua_newuserdata(ls, sizeof(tent->tnode.tupid));
	memcpy(stackid, &tent->tnode.tupid, sizeof(tent->tnode.tupid));
	lua_pushvalue(ls, lua_upvalueindex(1));
	lua_setmetatable(ls, 1);

	return 1;
}

static int tuplua_function_nodevariable_tostring(lua_State *ls)
{
	struct tupfile *tf = top_tupfile();
	int rc = -1;
	void *stackid;
	tupid_t tid;
	struct estring e;

	lua_settop(ls, 1);

	if(!lua_isuserdata(ls, 1))
		return luaL_error(ls, "Argument 1 is not a node variable.");
	stackid = lua_touserdata(ls, 1);
	tid = *(tupid_t *)stackid;

	if(estring_init(&e) < 0)
		return luaL_error(ls, "Error allocating memory in tuplua_function_nodevariable_tostring.");

	rc = get_relative_dir(NULL, &e, tf->curtent->tnode.tupid, tid);
	if(rc < 0)
		return luaL_error(ls, "Error getting relative path tuplua_function_nodevariable_tostring.");

	lua_settop(ls, 0);

	lua_pushlstring(ls, e.s, e.len);
	free(e.s);

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

int tup_lua_parser_new_state(void)
{
	gls = luaL_newstate();

	/* Register tup interaction functions in the "tup" table in Lua */
	lua_newtable(gls);
	tuplua_register_function(gls, "include", tuplua_function_include);
	tuplua_register_function(gls, "definerule", tuplua_function_definerule);
	tuplua_register_function(gls, "append_table", tuplua_function_append_table);
	tuplua_register_function(gls, "getcwd", tuplua_function_getcwd);
	tuplua_register_function(gls, "getvariantdir", tuplua_function_getvariantdir);
	tuplua_register_function(gls, "getvariantoutputdir", tuplua_function_getvariantoutputdir);
	tuplua_register_function(gls, "getdirectory", tuplua_function_getdirectory);
	tuplua_register_function(gls, "getrelativedir", tuplua_function_getrelativedir);
	tuplua_register_function(gls, "getconfig", tuplua_function_getconfig);
	tuplua_register_function(gls, "glob", tuplua_function_glob);
	tuplua_register_function(gls, "export", tuplua_function_export);
	tuplua_register_function(gls, "import", tuplua_function_import);
	tuplua_register_function(gls, "creategitignore", tuplua_function_creategitignore);
	tuplua_register_function(gls, "handle_fileread", tuplua_function_handle_fileread);
	tuplua_register_function(gls, "unchdir", tuplua_function_unchdir);
	tuplua_register_function(gls, "run", tuplua_function_run);

	lua_newtable(gls);
	lua_pushcfunction(gls, tuplua_function_nodevariable_tostring);
	lua_setfield(gls, -2, "__tostring");
	lua_pushcfunction(gls, tuplua_function_concat);
	lua_setfield(gls, -2, "__concat");
	lua_pushcclosure(gls, tuplua_function_nodevariable, 1);
	lua_setfield(gls, 1, "nodevariable");

	lua_setglobal(gls, "tup");

	/* Load some basic libraries.  Load the debug library so
	 * tracebacks for errors can be formatted nicely
	 */
	luaL_requiref(gls, "_G", luaopen_base, 1); lua_pop(gls, 1);
	luaL_requiref(gls, LUA_TABLIBNAME, luaopen_table, 1); lua_pop(gls, 1);
	luaL_requiref(gls, LUA_STRLIBNAME, luaopen_string, 1); lua_pop(gls, 1);
	luaL_requiref(gls, LUA_MATHLIBNAME, luaopen_math, 1); lua_pop(gls, 1);
	luaL_requiref(gls, LUA_DBLIBNAME, luaopen_debug, 1); lua_pop(gls, 1);
	luaL_requiref(gls, LUA_IOLIBNAME, luaopen_io, 1); lua_pop(gls, 1);
	luaL_requiref(gls, LUA_UTF8LIBNAME, luaopen_utf8, 1); lua_pop(gls, 1);
	lua_pushnil(gls); lua_setglobal(gls, "dofile");
	lua_pushnil(gls); lua_setglobal(gls, "loadfile");
	lua_pushnil(gls); lua_setglobal(gls, "load");
	lua_pushnil(gls); lua_setglobal(gls, "require");

	/* Load lua built-in lua helper functions from luabuiltin.h */
	lua_getglobal(gls, "debug");
	lua_getfield(gls, -1, "traceback");
	lua_setfield(gls, LUA_REGISTRYINDEX, "tup_traceback");
	lua_pop(gls, 1);
	lua_getfield(gls, LUA_REGISTRYINDEX, "tup_traceback");
	if(luaL_loadbuffer(gls, (char *)builtin_lua, builtin_lua_len, "builtin") != LUA_OK) {
		fprintf(stderr, "tup error: Failed to open builtins:\n%s\n", tuplua_tostring(gls, -1));
		lua_close(gls);
		return -1;
	}
	if(lua_pcall(gls, 0, 0, 1) != LUA_OK) {
		fprintf(stderr, "tup error: Failed to parse builtins:\n%s\n", tuplua_tostring(gls, -1));
		lua_close(gls);
		return -1;
	}
	lua_pop(gls, 1);

	return 0;
}

int parse_lua_include_rules(struct tupfile *tf)
{
	int top = lua_gettop(gls);
	if(parser_include_rules(tf, "Tuprules.lua") < 0) {
		if(tf->luaerror == TUPLUA_PENDINGERROR) {
			assert(lua_gettop(gls) == top + 2);
			fprintf(tf->f, "tup error %s\n", tuplua_tostring(gls, -1));
			lua_pop(gls, 1);
			tf->luaerror = TUPLUA_ERRORSHOWN;
		}
		return -1;
	}
	return 0;
}

int parse_lua_tupfile(struct tupfile *tf, struct buf *b, const char *name)
{
	struct tuplua_reader_data lrd = {b, 0};
	int top = lua_gettop(gls);

	lua_getfield(gls, LUA_REGISTRYINDEX, "tup_traceback");

	if(lua_load(gls, &tuplua_reader, &lrd, name, 0) != LUA_OK) {
		fprintf(tf->f, "tup error %s\n", tuplua_tostring(gls, -1));
		tf->luaerror = TUPLUA_PENDINGERROR;
		assert(lua_gettop(gls) == top + 2);
		return -1;
	}

	/* Override _ENV to use our per-Tupfile global variable sandboxing. */
	lua_getglobal(gls, "tup_environ");
	lua_setupvalue(gls, -2, 1);

	if(lua_pcall(gls, 0, 0, 1) != LUA_OK) {
		if(tf->luaerror != TUPLUA_ERRORSHOWN)
			fprintf(tf->f, "tup error %s\n", tuplua_tostring(gls, -1));
		tf->luaerror = TUPLUA_ERRORSHOWN;
		lua_pop(gls, 2);
		assert(lua_gettop(gls) == top);
		return -1;
	}

	lua_pop(gls, 1);
	return 0;
}

void tup_lua_parser_cleanup(void)
{
	if(gls)
		lua_close(gls);
}

void lua_parser_debug_run(void)
{
	debug_run = 1;
}

void push_tupfile(struct tupfile *tf)
{
	SLIST_INSERT_HEAD(&tupfile_list, tf, list);
	luaL_setoutput(gls, tf->f);
	lua_getglobal(gls, "tup_push_state");
	lua_call(gls, 0, 0);
}

void pop_tupfile(void)
{
	SLIST_REMOVE_HEAD(&tupfile_list, list);
	struct tupfile *tf = top_tupfile();
	if(tf) {
		luaL_setoutput(gls, tf->f);
	} else {
		luaL_setoutput(gls, stderr);
	}
	lua_getglobal(gls, "tup_pop_state");
	lua_call(gls, 0, 0);
}

static struct tupfile *top_tupfile(void)
{
	return SLIST_FIRST(&tupfile_list);
}

static void tup_string_to_lua(const char *value)
{
	/* Convert a tup space-separated string into a table with multiple
	 * values. The resulting table is left on the stack.
	 */
	const char *space;
	int idx = 1;

	if(!value) {
		lua_pushnil(gls);
		return;
	}
	lua_newtable(gls);
	lua_getglobal(gls, "tup_table_meta");
	lua_setmetatable(gls, -2);
	do {
		space = strchr(value, ' ');
		if(space) {
			lua_pushlstring(gls, value, space - value);
		} else {
			lua_pushstring(gls, value);
		}
		lua_rawseti(gls, -2, idx);
		idx++;
		if(space) {
			while(*space && isspace(*space))
				space++;
		}
		value = space;
	} while(space != NULL);
}

int luadb_set(const char *var, const char *value)
{
	lua_getglobal(gls, "tup_set_var");
	lua_pushstring(gls, var);
	tup_string_to_lua(value);
	lua_call(gls, 2, 0);
	return 0;
}

int luadb_append(const char *var, const char *value)
{
	lua_getglobal(gls, "tup_set_var");
	lua_pushstring(gls, var);
	lua_getglobal(gls, "tup_append_assignment");
	lua_getglobal(gls, "tup_get_var");
	lua_pushstring(gls, var);
	lua_call(gls, 1, 1);
	tup_string_to_lua(value);
	lua_call(gls, 2, 1);
	lua_call(gls, 2, 0);
	return 0;
}

int luadb_copy(const char *var, int varlen, struct estring *e)
{
	char buf[PATH_MAX];
	char *value;
	if(varlen+1 >= PATH_MAX) {
		fprintf(stderr, "tup error: Varname too long (%i bytes)\n", varlen);
		return -1;
	}
	strncpy(buf, var, varlen);
	buf[varlen] = 0;
	lua_getglobal(gls, "tup_get_var");
	lua_pushstring(gls, buf);
	lua_call(gls, 1, 1);
	if(lua_istable(gls, -1)) {
		value = tuplua_table_tostring(gls);
	} else {
		value = tuplua_strdup(gls, -1);
	}
	lua_pop(gls, 1);
	if(value) {
		if(estring_append(e, value, strlen(value)) < 0)
			return -1;
		free(value);
	}
	return 0;
}

static int get_path_list(struct tupfile *tf, const char *p, struct path_list_head *plist, int orderid)
{
	struct path_list *pl;

	pl = new_pl(tf, p, -1, &tf->bin_list, orderid);
	if(!pl)
		return -1;

	TAILQ_INSERT_TAIL(plist, pl, list);

	return 0;
}
