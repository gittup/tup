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

#include "environ.h"
#include "tupid_tree.h"
#include "entry.h"
#include "db.h"

static const char *default_env[] = {
	"PATH",
};

int environ_add_defaults(struct tupid_entries *root)
{
	unsigned int x;
	struct tup_entry *tent;
	for(x=0; x<sizeof(default_env) / sizeof(default_env[0]); x++) {
		if(tup_db_findenv(default_env[x], &tent) < 0)
			return -1;
		if(tupid_tree_add_dup(root, tent->tnode.tupid) < 0)
			return -1;
	}
	return 0;
}
