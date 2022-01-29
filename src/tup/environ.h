/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2022  Mike Shal <marfey@gmail.com>
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

#ifndef tup_environ_h
#define tup_environ_h

struct tent_entries;

int environ_add_defaults(struct tent_entries *root);

/* This returns the environment in the Windows-style format, where it is one
 * long string with a "\0\0" to terminate the array. Each individual environment
 * variable is nul-terminated. Eg:
 *
 * PATH=/bin\0FOO=fooval\0\0
 *
 * This way Windows can use it directly, and it is easier to serialize to the
 * master_fork server than a char**.
 *
 * The total size of the envblock is block_size bytes, and it contains num_entries
 * nul-terminated strings.
 */
struct tup_env {
	char *envblock;
	int block_size;
	int num_entries;
};

#define environ_free(te) free((te)->envblock)

#endif
