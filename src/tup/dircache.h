/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2023  Mike Shal <marfey@gmail.com>
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

#ifndef dircache_h
#define dircache_h

#include "tupid_tree.h"

/* The tupid in wd_node isn't a real tupid, but rather the watch descriptor
 * for inotify. The tupid in dt_node is the directory's tupid, like usual.
 */
struct dircache {
	struct tupid_tree wd_node;
	struct tupid_tree dt_node;
};

/* Contains the both trees for looking up dircache entries with either the
 * watch descriptor or the dir tupid.
 */
struct dircache_root {
	struct tupid_entries wd_root;
	struct tupid_entries dt_root;
};

/* Initializes the dircache_root structure */
void dircache_init(struct dircache_root *droot);

/* Adds the given wd -> dt relationship to the dircache.  */
void dircache_add(struct dircache_root *droot, int wd, tupid_t dt);

/* Returns the dircache given when wd was added to the dircache, or NULL if not
 * found.
 */
struct dircache *dircache_lookup_wd(struct dircache_root *droot, int wd);

/* Same as dircache_lookup_wd, only the dir tupid is used instead of the watch
 * descriptor.
 */
struct dircache *dircache_lookup_dt(struct dircache_root *droot, tupid_t dt);

/* Deletes dc from the dircache. */
void dircache_del(struct dircache_root *droot, struct dircache *dc);

#endif
