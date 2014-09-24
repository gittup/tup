/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010  James McKaskill
 * Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
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
#include "iat_patch.h"
#include "patch.h"
#include "trace.h"
#include <windows.h>
#include <psapi.h>

static void do_hook(void* fphook, void** fporig, IMAGE_THUNK_DATA* cur)
{
	DWORD old_protect;
	*fporig = (void*) cur->u1.Function;
	if(!VirtualProtect(cur, sizeof(IMAGE_THUNK_DATA), PAGE_EXECUTE_READWRITE, &old_protect)) {
		return;
	}

	cur->u1.Function = (DWORD_PTR)fphook;

	if(!VirtualProtect(cur, sizeof(IMAGE_THUNK_DATA), old_protect, &old_protect)) {
		return;
	}
}

static void hook(HMODULE h, const char *module_name, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur,
		 const struct patch_entry *begin, const struct patch_entry *end)
{
	if(orig->u1.Ordinal & IMAGE_ORDINAL_FLAG)
		return;

	IMAGE_IMPORT_BY_NAME* name = (IMAGE_IMPORT_BY_NAME*) (orig->u1.AddressOfData + (char*) h);
	const struct patch_entry *i;
	for(i = begin; i != end; i++) {
		if(i->skip)
			continue;
		if(stricmp(module_name, i->module))
			continue;
		if(strcmp((const char*)name->Name, i->name))
			continue;
		do_hook(i->new_proc, i->orig_proc, cur);
	}
}

static void foreach_module(HMODULE h, const struct patch_entry *begin, const struct patch_entry *end)
{
	IMAGE_DOS_HEADER* dos_header;
	IMAGE_NT_HEADERS* nt_headers;
	IMAGE_DATA_DIRECTORY* import_dir;
	IMAGE_IMPORT_DESCRIPTOR* imports;

	dos_header = (IMAGE_DOS_HEADER*) h;
	nt_headers = (IMAGE_NT_HEADERS*) (dos_header->e_lfanew + (char*) h);

	import_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	imports = (IMAGE_IMPORT_DESCRIPTOR*) (import_dir->VirtualAddress + (char*) h);
	if(import_dir->VirtualAddress == 0)
		return;

	while(imports->Name != 0) {
		char* dllname = (char*) h + imports->Name;
		if(imports->FirstThunk && imports->OriginalFirstThunk) {
			IMAGE_THUNK_DATA* cur = (IMAGE_THUNK_DATA*) (imports->FirstThunk + (char*) h);
			IMAGE_THUNK_DATA* orig = (IMAGE_THUNK_DATA*) (imports->OriginalFirstThunk + (char*) h);

			while(cur->u1.Function && orig->u1.Function) {
				hook(h, dllname, orig, cur, begin, end);
				cur++;
				orig++;
			}
		}
		imports++;
	}
}

int iat_patch(struct patch_entry *begin, struct patch_entry *end)
{
	DWORD modnum;
	HMODULE modules[256];
	char filename[MAX_PATH];

	if(!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &modnum))
		return -1;

	modnum /= sizeof(HMODULE);

	DWORD i;
	for(i = 0; i < modnum; i++) {
		if(!GetModuleFileNameA(modules[i], filename, sizeof(filename))) {
			return -1;
		}
		foreach_module(modules[i], begin, end);
	}

	return 0;
}
