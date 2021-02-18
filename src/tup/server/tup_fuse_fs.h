/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

#ifdef FUSE3
#define FUSE_USE_VERSION 30
#define mfiller(a, b, c, d) filler(a, b, c, d, FUSE_FILL_DIR_PLUS)
#else
#define FUSE_USE_VERSION 26
#define mfiller filler
#endif

#include <fuse.h>
#include "tup/tupid.h"

#define TUP_TMP ".tup/tmp"
#define TUP_JOB "@tupjob-"

struct file_info;

int tup_fuse_add_group(int id, struct file_info *finfo);
int tup_fuse_rm_group(struct file_info *finfo);
void tup_fuse_set_parser_mode(int mode);
int tup_fuse_server_get_dir_entries(const char *path, void *buf,
				    fuse_fill_dir_t filler);
void tup_fuse_fs_init(void);
int tup_fs_inited(void);
extern struct fuse_operations tup_fs_oper;

#endif
