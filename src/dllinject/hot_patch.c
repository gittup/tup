/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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
#include "hot_patch.h"
#include "patch.h"
#include "trace.h"
#include <windows.h>

static int hot_patch_int(void *old_proc, void *new_proc, void **orig_proc)
{
	static const BYTE signature[] = {
		0x90, // nop
		0x90,
		0x90,
		0x90,
		0x90,
		// <- function start here
		0x8b, 0xff // movl %edi,%edi
	};

	BYTE *begin = (BYTE*)old_proc - 5;

	DWORD old_protect;
	if(!VirtualProtect((PVOID)begin, sizeof(signature),
			   PAGE_EXECUTE_WRITECOPY, &old_protect))
		return -1;

	int ret = -1;
	if(memcmp((PVOID)begin, signature, sizeof(signature))) {
		DEBUG_HOOK("%x %x %x %x %x %x %x\n", begin[0], begin[1],begin[2],begin[3],begin[4],begin[5],begin[6]);
		goto exit;
	}

	*(begin + 0) = 0xe9; // long jump
	*(DWORD*)(begin + 1) = (DWORD_PTR)new_proc - (DWORD_PTR)old_proc;
	*(WORD*)(begin + 5) = 0xf9eb; // short jump back

	if(orig_proc)
		*orig_proc = (BYTE*)old_proc + 2;

	ret = 0;
exit:
     VirtualProtect(begin, sizeof(signature), old_protect, &old_protect);
     return ret;
}

int hot_patch(struct patch_entry *begin, struct patch_entry *end)
{
	struct patch_entry *i;
	for(i = begin; i != end; i++) {
		HMODULE mod = GetModuleHandle(i->module);
		if(!mod)
			continue;

		void *old_proc = GetProcAddress(mod, i->name);
		if(!old_proc)
			continue;

		if(hot_patch_int(old_proc, i->new_proc, i->orig_proc) != 0) {
			DEBUG_HOOK("not hot-patchable (%s)\n", i->name);
		} else {
			DEBUG_HOOK("hot-patched (%s)\n", i->name);
			i->skip = 1;
		}
	}

	return 0;
}
