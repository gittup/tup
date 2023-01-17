/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2013  Rendaw <rendaw@zarbosoft.com>
 * Copyright (C) 2013-2023  Mike Shal <marfey@gmail.com>
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

#ifndef tup_luaparser_h
#define tup_luaparser_h

struct tupfile;
struct buf;
struct estring;

int tup_lua_parser_new_state(void);
void tup_lua_parser_cleanup(void);
void lua_parser_debug_run(void);
int parse_lua_include_rules(struct tupfile *tf);
int parse_lua_tupfile(struct tupfile *tf, struct buf *b, const char *name);
void push_tupfile(struct tupfile *tf);
void pop_tupfile(void);
int luadb_set(const char *var, const char *value);
int luadb_append(const char *var, const char *value);
int luadb_copy(const char *var, int varlen, struct estring *e);

#endif
