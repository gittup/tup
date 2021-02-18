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

#include "environ.h"
#include "entry.h"
#include "db.h"
#include "tent_tree.h"

static const char *default_env[] = {
/* NOTE: Please increment PARSER_VERSION if these are modified */
	"PATH",
	"HOME",
#ifdef _WIN32
	/* Basic Windows variables */
	"SYSTEMROOT",
	/* Visual Studio variables */
	"DevEnvDir",
	"INCLUDE",
	"LIB",
	"LIBPATH",
	"TEMP",
	"TMP",
	"VCINSTALLDIR",
	"VS100COMNTOOLS",
	"VS90COMNTOOLS",
	"VSINSTALLDIR",
	"VCToolsInstallDir",
	"VCToolsRedistDir",
	"VCToolsVersion",
#endif
/* NOTE: Please increment PARSER_VERSION if these are modified */
};

int environ_add_defaults(struct tent_entries *root)
{
	unsigned int x;
	struct tup_entry *tent;
	for(x=0; x<sizeof(default_env) / sizeof(default_env[0]); x++) {
		if(tup_db_findenv(default_env[x], &tent) < 0)
			return -1;
		if(tent_tree_add_dup(root, tent) < 0)
			return -1;
	}
	return 0;
}
