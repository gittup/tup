/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2016  Mike Shal <marfey@gmail.com>
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

#ifndef tup_master_fork_h
#define tup_master_fork_h

#include "tup/compat.h"
#include "tup/tupid.h"

struct tup_env;

struct execmsg {
	tupid_t sid;
	int joblen;
	int dirlen;
	int cmdlen;
	int envlen;
	int vardictlen;
	int num_env_entries;
	int single_output;
	int need_namespacing;
	int run_in_bash;
};

#define JOB_MAX 64

int master_fork_exec(struct execmsg *em, const char *job, const char *dir,
		     const char *cmd, const char *newenv,
		     const char *vardict_file, int *status);

#endif
