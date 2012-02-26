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

#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"
#include <time.h>

#define TUP_CONFIG "tup.config"

struct tup_entry;
struct tup_entry_head;
struct path_element;
struct pel_group;

int create_name_file(tupid_t dt, const char *file, time_t mtime,
		     struct tup_entry **entry);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t tup_file_mod(tupid_t dt, const char *file);
tupid_t tup_file_mod_mtime(tupid_t dt, const char *file, time_t mtime,
			   int force);
int tup_file_del(tupid_t dt, const char *file, int len);
int tup_file_missing(struct tup_entry *tent);
int tup_del_id_force(tupid_t tupid, int type);
void tup_register_rmdir_callback(void (*callback)(tupid_t tupid));
struct tup_entry *get_tent_dt(tupid_t dt, const char *path);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir,
			  struct path_element **last, int sotgv, int full_deps);
tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     struct path_element **last, int sotgv, int full_deps);
int gimme_tent(const char *name, struct tup_entry **entry);

int delete_file(tupid_t dt, const char *name);
int delete_name_file(tupid_t tupid);

#endif
