/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011  Mike Shal <marfey@gmail.com>
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

#ifndef tup_fuse_fs_h
#define tup_fuse_fs_h

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include "tup/tupid.h"

#define TUP_TMP ".tup/tmp"
#define TUP_JOB "@tupjob-"

struct file_info;
struct tupid_entries;

int tup_fuse_add_group(int id, struct file_info *finfo);
int tup_fuse_rm_group(struct file_info *finfo);
void tup_fuse_set_parser_mode(int mode, struct tupid_entries *delete_root);
tupid_t tup_fuse_server_get_curid(void);
extern struct fuse_operations tup_fs_oper;

#endif
