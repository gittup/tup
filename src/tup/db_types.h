/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2013  Mike Shal <marfey@gmail.com>
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

#ifndef tup_db_types_h
#define tup_db_types_h

#define TUP_DIR ".tup"
#define TUP_DB_FILE ".tup/db"
#define DOT_DT 1

enum TUP_NODE_TYPE {
	TUP_NODE_FILE,
	TUP_NODE_CMD,
	TUP_NODE_DIR,
	TUP_NODE_VAR,
	TUP_NODE_GENERATED,
	TUP_NODE_GHOST,
	TUP_NODE_GROUP,
	TUP_NODE_ROOT,
};

enum TUP_FLAGS_TYPE {
	TUP_FLAGS_NONE=0,
	TUP_FLAGS_MODIFY=1,
	TUP_FLAGS_CREATE=2,
	TUP_FLAGS_CONFIG=4,
	TUP_FLAGS_VARIANT=8,
};

enum TUP_LINK_TYPE {
	TUP_LINK_NORMAL=1,
	TUP_LINK_STICKY=2,
	TUP_LINK_GROUP=3,
};

#endif
