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

#define BUILDING_DLLINJECT
#include "dllinject.h"
#include "tup/access_event.h"
#include "iat_patch.h"
#include "hot_patch.h"
#include "patch.h"
#include "trace.h"

#include <windows.h>
#include <ntdef.h>
#include <wow64.h>
#ifndef STATUS_SUCCESS
#include <ntstatus.h>
#endif
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <ctype.h>
#include <shlwapi.h>

#define __DBG_W64		0
#define __DBG_W32		0

#ifndef __in
#define __in
#define __out
#define __inout
#define __in_opt
#define __inout_opt
#define __reserved
#endif

typedef HFILE (WINAPI *OpenFile_t)(
    __in    LPCSTR lpFileName,
    __inout LPOFSTRUCT lpReOpenBuff,
    __in    UINT uStyle);

typedef HANDLE (WINAPI *CreateFileA_t)(
    __in     LPCSTR lpFileName,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwShareMode,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in     DWORD dwCreationDisposition,
    __in     DWORD dwFlagsAndAttributes,
    __in_opt HANDLE hTemplateFile);

typedef HANDLE (WINAPI *CreateFileW_t)(
    __in     LPCWSTR lpFileName,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwShareMode,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in     DWORD dwCreationDisposition,
    __in     DWORD dwFlagsAndAttributes,
    __in_opt HANDLE hTemplateFile);

typedef HANDLE (WINAPI *CreateFileTransactedA_t)(
    __in       LPCSTR lpFileName,
    __in       DWORD dwDesiredAccess,
    __in       DWORD dwShareMode,
    __in_opt   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in       DWORD dwCreationDisposition,
    __in       DWORD dwFlagsAndAttributes,
    __in_opt   HANDLE hTemplateFile,
    __in       HANDLE hTransaction,
    __in_opt   PUSHORT pusMiniVersion,
    __reserved PVOID  lpExtendedParameter);

typedef HANDLE (WINAPI *CreateFileTransactedW_t)(
    __in       LPCWSTR lpFileName,
    __in       DWORD dwDesiredAccess,
    __in       DWORD dwShareMode,
    __in_opt   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in       DWORD dwCreationDisposition,
    __in       DWORD dwFlagsAndAttributes,
    __in_opt   HANDLE hTemplateFile,
    __in       HANDLE hTransaction,
    __in_opt   PUSHORT pusMiniVersion,
    __reserved PVOID  lpExtendedParameter);

typedef BOOL (WINAPI *DeleteFileA_t)(
    __in LPCSTR lpFileName);

typedef BOOL (WINAPI *DeleteFileW_t)(
    __in LPCWSTR lpFileName);

typedef BOOL (WINAPI *DeleteFileTransactedA_t)(
    __in     LPCSTR lpFileName,
    __in     HANDLE hTransaction);

typedef BOOL (WINAPI *DeleteFileTransactedW_t)(
    __in     LPCWSTR lpFileName,
    __in     HANDLE hTransaction);

typedef BOOL (WINAPI *MoveFileA_t)(
    __in LPCSTR lpExistingFileName,
    __in LPCSTR lpNewFileName);

typedef BOOL (WINAPI *MoveFileW_t)(
    __in LPCWSTR lpExistingFileName,
    __in LPCWSTR lpNewFileName);

typedef BOOL (WINAPI *MoveFileExA_t)(
    __in     LPCSTR lpExistingFileName,
    __in_opt LPCSTR lpNewFileName,
    __in     DWORD    dwFlags);

typedef BOOL (WINAPI *MoveFileExW_t)(
    __in     LPCWSTR lpExistingFileName,
    __in_opt LPCWSTR lpNewFileName,
    __in     DWORD    dwFlags);

typedef BOOL (WINAPI *MoveFileWithProgressA_t)(
    __in     LPCSTR lpExistingFileName,
    __in_opt LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags);

typedef BOOL (WINAPI *MoveFileWithProgressW_t)(
    __in     LPCWSTR lpExistingFileName,
    __in_opt LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags);

typedef BOOL (WINAPI *MoveFileTransactedA_t)(
    __in     LPCSTR lpExistingFileName,
    __in_opt LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags,
    __in     HANDLE hTransaction);

typedef BOOL (WINAPI *MoveFileTransactedW_t)(
    __in     LPCWSTR lpExistingFileName,
    __in_opt LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags,
    __in     HANDLE hTransaction);

typedef BOOL (WINAPI *ReplaceFileA_t)(
    __in       LPCSTR  lpReplacedFileName,
    __in       LPCSTR  lpReplacementFileName,
    __in_opt   LPCSTR  lpBackupFileName,
    __in       DWORD   dwReplaceFlags,
    __reserved LPVOID  lpExclude,
    __reserved LPVOID  lpReserved);

typedef BOOL (WINAPI *ReplaceFileW_t)(
    __in       LPCWSTR lpReplacedFileName,
    __in       LPCWSTR lpReplacementFileName,
    __in_opt   LPCWSTR lpBackupFileName,
    __in       DWORD   dwReplaceFlags,
    __reserved LPVOID  lpExclude,
    __reserved LPVOID  lpReserved);

typedef BOOL (WINAPI *CopyFileA_t)(
    __in LPCSTR lpExistingFileName,
    __in LPCSTR lpNewFileName,
    __in BOOL bFailIfExists);

typedef BOOL (WINAPI *CopyFileW_t)(
    __in LPCWSTR lpExistingFileName,
    __in LPCWSTR lpNewFileName,
    __in BOOL bFailIfExists);

typedef BOOL (WINAPI *CopyFileExA_t)(
    __in     LPCSTR lpExistingFileName,
    __in     LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags);

typedef BOOL (WINAPI *CopyFileExW_t)(
    __in     LPCWSTR lpExistingFileName,
    __in     LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags);

typedef BOOL (WINAPI *CopyFileTransactedA_t)(
    __in     LPCSTR lpExistingFileName,
    __in     LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags,
    __in     HANDLE hTransaction);

typedef BOOL (WINAPI *CopyFileTransactedW_t)(
    __in     LPCWSTR lpExistingFileName,
    __in     LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags,
    __in     HANDLE hTransaction);

typedef DWORD (WINAPI *GetFileAttributesA_t)(
    __in LPCSTR lpFileName);

typedef DWORD (WINAPI *GetFileAttributesW_t)(
    __in LPCWSTR lpFileName);

typedef BOOL (WINAPI *GetFileAttributesExA_t)(
    __in  LPCSTR lpFileName,
    __in  GET_FILEEX_INFO_LEVELS fInfoLevelId,
    __out LPVOID lpFileInformation);

typedef BOOL (WINAPI *GetFileAttributesExW_t)(
    __in  LPCWSTR lpFileName,
    __in  GET_FILEEX_INFO_LEVELS fInfoLevelId,
    __out LPVOID lpFileInformation);

typedef __out HANDLE (WINAPI *FindFirstFileA_t)(
    __in  LPCSTR lpFileName,
    __out LPWIN32_FIND_DATAA lpFindFileData);

typedef __out HANDLE (WINAPI *FindFirstFileW_t)(
    __in  LPCWSTR lpFileName,
    __out LPWIN32_FIND_DATAW lpFindFileData);

typedef BOOL (WINAPI *FindNextFileA_t)(
    __in  HANDLE hFindFile,
    __out LPWIN32_FIND_DATAA lpFindFileData);

typedef BOOL (WINAPI *FindNextFileW_t)(
    __in  HANDLE hFindFile,
    __out LPWIN32_FIND_DATAW lpFindFileData);

typedef BOOL (WINAPI *CreateProcessA_t)(
    __in_opt    LPCSTR lpApplicationName,
    __inout_opt LPSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOA lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL (WINAPI * CreateProcessW_t)(
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL (WINAPI *CreateProcessAsUserA_t)(
    __in_opt    HANDLE hToken,
    __in_opt    LPCSTR lpApplicationName,
    __inout_opt LPSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOA lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL (WINAPI *CreateProcessAsUserW_t)(
    __in_opt    HANDLE hToken,
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation
    );

typedef BOOL (WINAPI *CreateProcessWithLogonW_t)(
    __in        LPCWSTR lpUsername,
    __in_opt    LPCWSTR lpDomain,
    __in        LPCWSTR lpPassword,
    __in        DWORD dwLogonFlags,
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL (WINAPI *CreateProcessWithTokenW_t)(
    __in        HANDLE hToken,
    __in        DWORD dwLogonFlags,
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation);

typedef void *PIO_STATUS_BLOCK;
typedef NTSTATUS (WINAPI *NtOpenFile_t)(
    __out  PHANDLE FileHandle,
    __in   ACCESS_MASK DesiredAccess,
    __in   POBJECT_ATTRIBUTES ObjectAttributes,
    __out  PIO_STATUS_BLOCK IoStatusBlock,
    __in   ULONG ShareAccess,
    __in   ULONG OpenOptions);

typedef NTSTATUS (WINAPI *NtCreateFile_t)(
    __out     PHANDLE FileHandle,
    __in      ACCESS_MASK DesiredAccess,
    __in      POBJECT_ATTRIBUTES ObjectAttributes,
    __out     PIO_STATUS_BLOCK IoStatusBlock,
    __in_opt  PLARGE_INTEGER AllocationSize,
    __in      ULONG FileAttributes,
    __in      ULONG ShareAccess,
    __in      ULONG CreateDisposition,
    __in      ULONG CreateOptions,
    __in      PVOID EaBuffer,
    __in      ULONG EaLength);

typedef int (*access_t)(const char *pathname, int mode);
typedef FILE *(*fopen_t)(const char *path, const char *mode);
typedef int (*rename_t)(const char *oldpath, const char *newpath);
typedef int (*remove_t)(const char *pathname);

static OpenFile_t			OpenFile_orig;
static CreateFileA_t			CreateFileA_orig;
static CreateFileW_t			CreateFileW_orig;
static CreateFileTransactedA_t		CreateFileTransactedA_orig;
static CreateFileTransactedW_t		CreateFileTransactedW_orig;
static DeleteFileA_t			DeleteFileA_orig;
static DeleteFileW_t			DeleteFileW_orig;
static DeleteFileTransactedA_t		DeleteFileTransactedA_orig;
static DeleteFileTransactedW_t		DeleteFileTransactedW_orig;
static MoveFileA_t			MoveFileA_orig;
static MoveFileW_t			MoveFileW_orig;
static MoveFileExA_t			MoveFileExA_orig;
static MoveFileExW_t			MoveFileExW_orig;
static MoveFileWithProgressA_t		MoveFileWithProgressA_orig;
static MoveFileWithProgressW_t		MoveFileWithProgressW_orig;
static MoveFileTransactedA_t		MoveFileTransactedA_orig;
static MoveFileTransactedW_t		MoveFileTransactedW_orig;
static ReplaceFileA_t			ReplaceFileA_orig;
static ReplaceFileW_t			ReplaceFileW_orig;
static CopyFileA_t			CopyFileA_orig;
static CopyFileW_t			CopyFileW_orig;
static CopyFileExA_t			CopyFileExA_orig;
static CopyFileExW_t			CopyFileExW_orig;
static CopyFileTransactedA_t		CopyFileTransactedA_orig;
static CopyFileTransactedW_t		CopyFileTransactedW_orig;
static GetFileAttributesA_t		GetFileAttributesA_orig;
static GetFileAttributesW_t		GetFileAttributesW_orig;
static GetFileAttributesExA_t		GetFileAttributesExA_orig;
static GetFileAttributesExW_t		GetFileAttributesExW_orig;
static FindFirstFileA_t			FindFirstFileA_orig;
static FindFirstFileW_t			FindFirstFileW_orig;
static FindNextFileA_t			FindNextFileA_orig;
static FindNextFileW_t			FindNextFileW_orig;
static CreateProcessA_t			CreateProcessA_orig;
static CreateProcessW_t			CreateProcessW_orig;
static CreateProcessAsUserA_t		CreateProcessAsUserA_orig;
static CreateProcessAsUserW_t		CreateProcessAsUserW_orig;
static CreateProcessWithLogonW_t	CreateProcessWithLogonW_orig;
static CreateProcessWithTokenW_t	CreateProcessWithTokenW_orig;
static NtCreateFile_t			NtCreateFile_orig;
static NtOpenFile_t			NtOpenFile_orig;
static access_t				_access_orig;
static fopen_t				fopen_orig;
static rename_t				rename_orig;
static remove_t				remove_orig;

#define TUP_CREATE_WRITE_FLAGS (GENERIC_WRITE | FILE_APPEND_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES)
/* Including ddk/wdm.h causes other issues, and this is all we need... */
#define FILE_OPEN_FOR_BACKUP_INTENT 0x00004000

#define handle_file(a, b, c) mhandle_file(a, b, c, __LINE__)
static void mhandle_file(const char* file, const char* file2, enum access_type at, int line);
static void handle_file_w(const wchar_t* file, const wchar_t* file2, enum access_type at);

static const char *strcasestr(const char *arg1, const char *arg2);
static const wchar_t *wcscasestr(const wchar_t *arg1, const wchar_t *arg2);

static char s_depfilename[PATH_MAX];
static char s_vardict_file[PATH_MAX];
static HANDLE deph = INVALID_HANDLE_VALUE;
static HANDLE vardicth = INVALID_HANDLE_VALUE;

static int writef(const char *data, unsigned int len)
{
	int rc = 0;
	DWORD num_written;

	if(!WriteFile(deph, data, len, &num_written, NULL)) {
		DEBUG_HOOK("failed to write %i bytes\n", len);
		rc = -1;
	}
	if(num_written != len) {
		DEBUG_HOOK("failed to write exactly %i bytes\n", len);
		rc = -1;
	}
	return rc;
}

/* -------------------------------------------------------------------------- */

static HFILE WINAPI OpenFile_hook(
    __in    LPCSTR lpFileName,
    __inout LPOFSTRUCT lpReOpenBuff,
    __in    UINT uStyle)
{
	if (uStyle & OF_DELETE) {
		handle_file(lpFileName, NULL, ACCESS_UNLINK);
	} else if (uStyle & (OF_READWRITE | OF_WRITE | OF_SHARE_DENY_WRITE | OF_SHARE_EXCLUSIVE | OF_CREATE)) {
		handle_file(lpFileName, NULL, ACCESS_WRITE);
	} else {
		handle_file(lpFileName, NULL, ACCESS_READ);
	}

	return OpenFile_orig(
		lpFileName,
		lpReOpenBuff,
		uStyle);
}

/* -------------------------------------------------------------------------- */

static HANDLE WINAPI CreateFileA_hook(
    __in     LPCSTR lpFileName,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwShareMode,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in     DWORD dwCreationDisposition,
    __in     DWORD dwFlagsAndAttributes,
    __in_opt HANDLE hTemplateFile)
{
	HANDLE h = CreateFileA_orig(
		lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile);

	DEBUG_HOOK("CreateFileA '%s', %p:%x, %x, %x, %x, %x\n",
		lpFileName,
		h,
		GetLastError(),
		dwDesiredAccess,
		dwShareMode,
		dwCreationDisposition,
		dwFlagsAndAttributes);

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & TUP_CREATE_WRITE_FLAGS) {
		handle_file(lpFileName, NULL, ACCESS_WRITE);
	} else {
		handle_file(lpFileName, NULL, ACCESS_READ);
	}

	return h;
}


/* -------------------------------------------------------------------------- */

static HANDLE WINAPI CreateFileW_hook(
    __in     LPCWSTR lpFileName,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwShareMode,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in     DWORD dwCreationDisposition,
    __in     DWORD dwFlagsAndAttributes,
    __in_opt HANDLE hTemplateFile)
{
	HANDLE h = CreateFileW_orig(
		lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile);

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & TUP_CREATE_WRITE_FLAGS) {
		handle_file_w(lpFileName, NULL, ACCESS_WRITE);
	} else {
		handle_file_w(lpFileName, NULL, ACCESS_READ);
	}

	return h;
}

/* -------------------------------------------------------------------------- */

HANDLE WINAPI CreateFileTransactedA_hook(
    __in       LPCSTR lpFileName,
    __in       DWORD dwDesiredAccess,
    __in       DWORD dwShareMode,
    __in_opt   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in       DWORD dwCreationDisposition,
    __in       DWORD dwFlagsAndAttributes,
    __in_opt   HANDLE hTemplateFile,
    __in       HANDLE hTransaction,
    __in_opt   PUSHORT pusMiniVersion,
    __reserved PVOID  lpExtendedParameter)
{
	HANDLE h = CreateFileTransactedA_orig(
		lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile,
		hTransaction,
		pusMiniVersion,
		lpExtendedParameter);

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & TUP_CREATE_WRITE_FLAGS) {
		handle_file(lpFileName, NULL, ACCESS_WRITE);
	} else {
		handle_file(lpFileName, NULL, ACCESS_READ);
	}

	return h;
}

HANDLE WINAPI CreateFileTransactedW_hook(
    __in       LPCWSTR lpFileName,
    __in       DWORD dwDesiredAccess,
    __in       DWORD dwShareMode,
    __in_opt   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in       DWORD dwCreationDisposition,
    __in       DWORD dwFlagsAndAttributes,
    __in_opt   HANDLE hTemplateFile,
    __in       HANDLE hTransaction,
    __in_opt   PUSHORT pusMiniVersion,
    __reserved PVOID  lpExtendedParameter)
{
	HANDLE h = CreateFileTransactedW_orig(
		lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile,
		hTransaction,
		pusMiniVersion,
		lpExtendedParameter);

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & TUP_CREATE_WRITE_FLAGS) {
		handle_file_w(lpFileName, NULL, ACCESS_WRITE);
	} else {
		handle_file_w(lpFileName, NULL, ACCESS_READ);
	}

	return h;
}

static char *unicode_to_ansi(PUNICODE_STRING uni)
{
	int len;
	char *name = NULL;

	len = WideCharToMultiByte(CP_UTF8, 0, uni->Buffer, uni->Length / sizeof(wchar_t), 0, 0, NULL, NULL);
	if(len > 0) {
		name = malloc(len + 1);
		WideCharToMultiByte(CP_UTF8, 0, uni->Buffer, uni->Length / sizeof(wchar_t), name, len, NULL, NULL);
		name[len] = 0;
	}
	return name;
}

NTSTATUS WINAPI NtCreateFile_hook(
    __out     PHANDLE FileHandle,
    __in      ACCESS_MASK DesiredAccess,
    __in      POBJECT_ATTRIBUTES ObjectAttributes,
    __out     PIO_STATUS_BLOCK IoStatusBlock,
    __in_opt  PLARGE_INTEGER AllocationSize,
    __in      ULONG FileAttributes,
    __in      ULONG ShareAccess,
    __in      ULONG CreateDisposition,
    __in      ULONG CreateOptions,
    __in      PVOID EaBuffer,
    __in      ULONG EaLength)
{
	NTSTATUS rc = NtCreateFile_orig(FileHandle,
					DesiredAccess,
					ObjectAttributes,
					IoStatusBlock,
					AllocationSize,
					FileAttributes,
					ShareAccess,
					CreateDisposition,
					CreateOptions,
					EaBuffer,
					EaLength);
	char *ansi;

	ansi = unicode_to_ansi(ObjectAttributes->ObjectName);

	if(ansi)  {
		const char *name = ansi;

		DEBUG_HOOK("NtCreateFile[%i] '%s': %x, %x, %x\n", rc, ansi, ShareAccess, DesiredAccess, CreateOptions);
		if(strncmp(name, "\\??\\", 4) == 0) {
			name += 4;
			/* Windows started trying to read a file called
			 * "\??\Ip", which broke some of the tests. This just
			 * skips anything that doesn't begin with something
			 * like "C:"
			 */
			if(name[0] != 0 && name[1] != ':')
				goto out_free;
		}

		if (rc == STATUS_SUCCESS && DesiredAccess & TUP_CREATE_WRITE_FLAGS) {
			handle_file(name, NULL, ACCESS_WRITE);
		} else {
			handle_file(name, NULL, ACCESS_READ);
		}
out_free:
		free(ansi);
	}

	return rc;
}

NTSTATUS WINAPI NtOpenFile_hook(
    __out  PHANDLE FileHandle,
    __in   ACCESS_MASK DesiredAccess,
    __in   POBJECT_ATTRIBUTES ObjectAttributes,
    __out  PIO_STATUS_BLOCK IoStatusBlock,
    __in   ULONG ShareAccess,
    __in   ULONG OpenOptions)
{
	NTSTATUS rc = NtOpenFile_orig(FileHandle,
				      DesiredAccess,
				      ObjectAttributes,
				      IoStatusBlock,
				      ShareAccess,
				      OpenOptions);
	char *ansi;

	ansi = unicode_to_ansi(ObjectAttributes->ObjectName);

	if(ansi) {
		const char *name = ansi;

		DEBUG_HOOK("NtOpenFile[%i] '%s': %x, %x, %x\n", rc, ansi, ShareAccess, DesiredAccess, OpenOptions);
		if(strncmp(name, "\\??\\", 4) == 0) {
			name += 4;
			/* Windows started trying to read a file called "\??\Ip",
			 * which broke some of the tests. This just skips
			 * anything that doesn't begin with something like "C:"
			 */
			if(name[0] != 0 && name[1] != ':')
				goto out_free;
		}

		/* The ShareAccess == FILE_SHARE_DELETE check might be
		 * specific to how cygwin handles unlink(). It is very
		 * confusing to follow, but it doesn't ever seem to go through
		 * the DeleteFile() route. This is the only place I've found
		 * that seems to be able to hook those events.
		 *
		 * The DesiredAccess & DELETE check is how cygwin does a
		 * rename() to remove the old file.
		 */
		if(ShareAccess == FILE_SHARE_DELETE ||
		   DesiredAccess & DELETE) {
			handle_file(name, NULL, ACCESS_UNLINK);
		} else if(OpenOptions & FILE_OPEN_FOR_BACKUP_INTENT) {
			/* The MSVC linker seems to successfully open
			 * "prog.ilk" for reading (when linking "prog.exe"),
			 * even though no such file exists. This confuses tup.
			 * It seems that this flag is used for temporary files,
			 * so that should be safe to ignore.
			 */
		} else {
			if (rc == STATUS_SUCCESS && DesiredAccess & TUP_CREATE_WRITE_FLAGS) {
				handle_file(name, NULL, ACCESS_WRITE);
			} else {
				handle_file(name, NULL, ACCESS_READ);
			}
		}
out_free:
		free(ansi);
	}

	return rc;
}

BOOL WINAPI DeleteFileA_hook(
    __in LPCSTR lpFileName)
{
	handle_file(lpFileName, NULL, ACCESS_UNLINK);
	return DeleteFileA_orig(lpFileName);
}

BOOL WINAPI DeleteFileW_hook(
    __in LPCWSTR lpFileName)
{
	handle_file_w(lpFileName, NULL, ACCESS_UNLINK);
	return DeleteFileW_orig(lpFileName);
}

BOOL WINAPI DeleteFileTransactedA_hook(
    __in     LPCSTR lpFileName,
    __in     HANDLE hTransaction)
{
	handle_file(lpFileName, NULL, ACCESS_UNLINK);
	return DeleteFileTransactedA_orig(lpFileName, hTransaction);
}

BOOL WINAPI DeleteFileTransactedW_hook(
    __in     LPCWSTR lpFileName,
    __in     HANDLE hTransaction)
{
	handle_file_w(lpFileName, NULL, ACCESS_UNLINK);
	return DeleteFileTransactedW_orig(lpFileName, hTransaction);
}

BOOL WINAPI MoveFileA_hook(
    __in LPCSTR lpExistingFileName,
    __in LPCSTR lpNewFileName)
{
	handle_file(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileA_orig(lpExistingFileName, lpNewFileName);
}

BOOL WINAPI MoveFileW_hook(
    __in LPCWSTR lpExistingFileName,
    __in LPCWSTR lpNewFileName)
{
	handle_file_w(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileW_orig(lpExistingFileName, lpNewFileName);
}

BOOL WINAPI MoveFileExA_hook(
    __in     LPCSTR lpExistingFileName,
    __in_opt LPCSTR lpNewFileName,
    __in     DWORD    dwFlags)
{
	handle_file(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileExA_orig(lpExistingFileName, lpNewFileName, dwFlags);
}

BOOL WINAPI MoveFileExW_hook(
    __in     LPCWSTR lpExistingFileName,
    __in_opt LPCWSTR lpNewFileName,
    __in     DWORD    dwFlags)
{
	handle_file_w(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileExW_orig(lpExistingFileName, lpNewFileName, dwFlags);
}

BOOL WINAPI MoveFileWithProgressA_hook(
    __in     LPCSTR lpExistingFileName,
    __in_opt LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags)
{
	handle_file(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileWithProgressA_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		dwFlags);
}

BOOL WINAPI MoveFileWithProgressW_hook(
    __in     LPCWSTR lpExistingFileName,
    __in_opt LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags)
{
	handle_file_w(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileWithProgressW_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		dwFlags);
}

BOOL WINAPI MoveFileTransactedA_hook(
    __in     LPCSTR lpExistingFileName,
    __in_opt LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags,
    __in     HANDLE hTransaction)
{
	handle_file(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileTransactedA_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		dwFlags,
		hTransaction);
}

BOOL WINAPI MoveFileTransactedW_hook(
    __in     LPCWSTR lpExistingFileName,
    __in_opt LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in     DWORD dwFlags,
    __in     HANDLE hTransaction)
{
	handle_file_w(lpExistingFileName, lpNewFileName, ACCESS_RENAME);
	return MoveFileTransactedW_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		dwFlags,
		hTransaction);
}

BOOL WINAPI ReplaceFileA_hook(
    __in       LPCSTR  lpReplacedFileName,
    __in       LPCSTR  lpReplacementFileName,
    __in_opt   LPCSTR  lpBackupFileName,
    __in       DWORD   dwReplaceFlags,
    __reserved LPVOID  lpExclude,
    __reserved LPVOID  lpReserved)
{
	handle_file(lpReplacementFileName, lpReplacedFileName, ACCESS_RENAME);
	return ReplaceFileA_orig(
		lpReplacedFileName,
		lpReplacementFileName,
		lpBackupFileName,
		dwReplaceFlags,
		lpExclude,
		lpReserved);
}

BOOL WINAPI ReplaceFileW_hook(
    __in       LPCWSTR lpReplacedFileName,
    __in       LPCWSTR lpReplacementFileName,
    __in_opt   LPCWSTR lpBackupFileName,
    __in       DWORD   dwReplaceFlags,
    __reserved LPVOID  lpExclude,
    __reserved LPVOID  lpReserved)
{
	handle_file_w(lpReplacementFileName, lpReplacedFileName, ACCESS_RENAME);
	return ReplaceFileW_orig(
		lpReplacedFileName,
		lpReplacementFileName,
		lpBackupFileName,
		dwReplaceFlags,
		lpExclude,
		lpReserved);
}

BOOL WINAPI CopyFileA_hook(
    __in LPCSTR lpExistingFileName,
    __in LPCSTR lpNewFileName,
    __in BOOL bFailIfExists)
{
	handle_file(lpExistingFileName, NULL, ACCESS_READ);
	handle_file(lpNewFileName, NULL, ACCESS_WRITE);
	return CopyFileA_orig(
		lpExistingFileName,
		lpNewFileName,
		bFailIfExists);
}

BOOL WINAPI CopyFileW_hook(
    __in LPCWSTR lpExistingFileName,
    __in LPCWSTR lpNewFileName,
    __in BOOL bFailIfExists)
{
	handle_file_w(lpExistingFileName, NULL, ACCESS_READ);
	handle_file_w(lpNewFileName, NULL, ACCESS_WRITE);
	return CopyFileW_orig(
		lpExistingFileName,
		lpNewFileName,
		bFailIfExists);
}

BOOL WINAPI CopyFileExA_hook(
    __in     LPCSTR lpExistingFileName,
    __in     LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags)
{
	handle_file(lpExistingFileName, NULL, ACCESS_READ);
	handle_file(lpNewFileName, NULL, ACCESS_WRITE);
	return CopyFileExA_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		pbCancel,
		dwCopyFlags);
}

BOOL WINAPI CopyFileExW_hook(
    __in     LPCWSTR lpExistingFileName,
    __in     LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags)
{
	handle_file_w(lpExistingFileName, NULL, ACCESS_READ);
	handle_file_w(lpNewFileName, NULL, ACCESS_WRITE);
	return CopyFileExW_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		pbCancel,
		dwCopyFlags);
}

BOOL WINAPI CopyFileTransactedA_hook(
    __in     LPCSTR lpExistingFileName,
    __in     LPCSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags,
    __in     HANDLE hTransaction)
{
	handle_file(lpExistingFileName, NULL, ACCESS_READ);
	handle_file(lpNewFileName, NULL, ACCESS_WRITE);
	return CopyFileTransactedA_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		pbCancel,
		dwCopyFlags,
		hTransaction);
}

BOOL WINAPI CopyFileTransactedW_hook(
    __in     LPCWSTR lpExistingFileName,
    __in     LPCWSTR lpNewFileName,
    __in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
    __in_opt LPVOID lpData,
    __in_opt LPBOOL pbCancel,
    __in     DWORD dwCopyFlags,
    __in     HANDLE hTransaction)
{
	handle_file_w(lpExistingFileName, NULL, ACCESS_READ);
	handle_file_w(lpNewFileName, NULL, ACCESS_WRITE);
	return CopyFileTransactedW_orig(
		lpExistingFileName,
		lpNewFileName,
		lpProgressRoutine,
		lpData,
		pbCancel,
		dwCopyFlags,
		hTransaction);
}

#define ATTRIB_FAIL 0xffffffff
DWORD WINAPI GetFileAttributesA_hook(
    __in LPCSTR lpFileName)
{
	DWORD attributes = GetFileAttributesA_orig(lpFileName);
	DEBUG_HOOK("GetFileAttributesA '%s'\n", lpFileName);

	/* If it fails (attributes == -1), we need to handle the read since
	 * it will be a ghost. If the file exists, we only care if it's a file
	 * and not a directory.
	 */
	if(attributes == ATTRIB_FAIL || ! (attributes & FILE_ATTRIBUTE_DIRECTORY))
		handle_file(lpFileName, NULL, ACCESS_READ);
	return attributes;
}

DWORD WINAPI GetFileAttributesW_hook(
    __in LPCWSTR lpFileName)
{
	DWORD attributes = GetFileAttributesW_orig(lpFileName);
	if(attributes == ATTRIB_FAIL || ! (attributes & FILE_ATTRIBUTE_DIRECTORY))
		handle_file_w(lpFileName, NULL, ACCESS_READ);
	return attributes;
}

BOOL WINAPI GetFileAttributesExA_hook(
    __in  LPCSTR lpFileName,
    __in  GET_FILEEX_INFO_LEVELS fInfoLevelId,
    __out LPVOID lpFileInformation)
{
	DWORD attributes = GetFileAttributesExA_orig(
		lpFileName,
		fInfoLevelId,
		lpFileInformation);
	if(attributes == ATTRIB_FAIL || ! (attributes & FILE_ATTRIBUTE_DIRECTORY))
		handle_file(lpFileName, NULL, ACCESS_READ);
	return attributes;
}

BOOL WINAPI GetFileAttributesExW_hook(
    __in  LPCWSTR lpFileName,
    __in  GET_FILEEX_INFO_LEVELS fInfoLevelId,
    __out LPVOID lpFileInformation)
{
	DWORD attributes = GetFileAttributesExW_orig(
		lpFileName,
		fInfoLevelId,
		lpFileInformation);
	if(attributes == ATTRIB_FAIL || ! (attributes & FILE_ATTRIBUTE_DIRECTORY))
		handle_file_w(lpFileName, NULL, ACCESS_READ);
	return attributes;
}

__out HANDLE WINAPI FindFirstFileA_hook(
    __in  LPCSTR lpFileName,
    __out LPWIN32_FIND_DATAA lpFindFileData)
{
	DEBUG_HOOK("FindFirstFileA '%s'\n", lpFileName);
	handle_file(lpFileName, NULL, ACCESS_READ);
	return FindFirstFileA_orig(lpFileName, lpFindFileData);
}

__out HANDLE WINAPI FindFirstFileW_hook(
    __in  LPCWSTR lpFileName,
    __out LPWIN32_FIND_DATAW lpFindFileData)
{
	DEBUG_HOOK("FindFirstFileW '%S'\n", lpFileName);
	handle_file_w(lpFileName, NULL, ACCESS_READ);
	return FindFirstFileW_orig(lpFileName, lpFindFileData);
}

BOOL WINAPI FindNextFileA_hook(
    __in  HANDLE hFindFile,
    __out LPWIN32_FIND_DATAA lpFindFileData)
{
	if (!FindNextFileA_orig(hFindFile, lpFindFileData))
		return 0;

	DEBUG_HOOK("FindNextFileA '%s'\n", lpFindFileData->cFileName);
	return 1;
}

BOOL WINAPI FindNextFileW_hook(
    __in  HANDLE hFindFile,
    __out LPWIN32_FIND_DATAW lpFindFileData)
{
	if (!FindNextFileW_orig(hFindFile, lpFindFileData))
		return 0;

	DEBUG_HOOK("FindNextFileW '%S'\n", lpFindFileData->cFileName);
	return 1;
}

BOOL WINAPI CreateProcessA_hook(
    __in_opt    LPCSTR lpApplicationName,
    __inout_opt LPSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOA lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL ret = CreateProcessA_orig(
		lpApplicationName,
		lpCommandLine,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation);

	DEBUG_HOOK("CreateProcessA '%s' '%s' in '%s'\n",
		lpApplicationName,
		lpCommandLine,
		lpCurrentDirectory);

	if (!ret) {
		return 0;
	}

	/* Ignore mspdbsrv.exe, since it continues to run in the background */
	if(!lpApplicationName || strcasestr(lpApplicationName, "mspdbsrv.exe") == NULL
	   || strcasestr(lpApplicationName, "tup32detect.exe") == NULL) {
		tup_inject_dll(lpProcessInformation, s_depfilename, s_vardict_file);
	}

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

BOOL WINAPI CreateProcessW_hook(
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL ret = CreateProcessW_orig(
		lpApplicationName,
		lpCommandLine,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation);

	DEBUG_HOOK("CreateProcessW %x '%S' '%S' in '%S'\n",
		dwCreationFlags,
		lpApplicationName,
		lpCommandLine,
		lpCurrentDirectory);

	if (!ret) {
		return 0;
	}

	/* Ignore mspdbsrv.exe, since it continues to run in the background */
	if(!lpApplicationName || wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL
	   || wcscasestr(lpApplicationName, L"tup32detect.exe") == NULL) {
		tup_inject_dll(lpProcessInformation, s_depfilename, s_vardict_file);
	}

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

BOOL WINAPI CreateProcessAsUserA_hook(
    __in_opt    HANDLE hToken,
    __in_opt    LPCSTR lpApplicationName,
    __inout_opt LPSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOA lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL ret = CreateProcessAsUserA_orig(
		hToken,
		lpApplicationName,
		lpCommandLine,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation);

	DEBUG_HOOK("CreateProcessAsUserA '%s' '%s' in '%s'\n",
		lpApplicationName,
		lpCommandLine,
		lpCurrentDirectory);

	if (!ret) {
		return 0;
	}

	/* Ignore mspdbsrv.exe, since it continues to run in the background */
	if(!lpApplicationName || strcasestr(lpApplicationName, "mspdbsrv.exe") == NULL
	   || strcasestr(lpApplicationName, "tup32detect.exe") == NULL) {
		tup_inject_dll(lpProcessInformation, s_depfilename, s_vardict_file);
	}

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

BOOL WINAPI CreateProcessAsUserW_hook(
    __in_opt    HANDLE hToken,
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL ret = CreateProcessAsUserW_orig(
		hToken,
		lpApplicationName,
		lpCommandLine,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation);

	DEBUG_HOOK("CreateProcessAsUserW '%S' '%S' in '%S'\n",
		lpApplicationName,
		lpCommandLine,
		lpCurrentDirectory);

	if (!ret) {
		return 0;
	}

	/* Ignore mspdbsrv.exe, since it continues to run in the background */
	if(!lpApplicationName || wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL
	   || wcscasestr(lpApplicationName, L"tup32detect.exe") == NULL) {
		tup_inject_dll(lpProcessInformation, s_depfilename, s_vardict_file);
	}

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

BOOL WINAPI CreateProcessWithLogonW_hook(
    __in        LPCWSTR lpUsername,
    __in_opt    LPCWSTR lpDomain,
    __in        LPCWSTR lpPassword,
    __in        DWORD dwLogonFlags,
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL ret = CreateProcessWithLogonW_orig(
		lpUsername,
		lpDomain,
		lpPassword,
		dwLogonFlags,
		lpApplicationName,
		lpCommandLine,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation);

	DEBUG_HOOK("CreateProcessWithLogonW '%S' '%S' in '%S'\n",
		lpApplicationName,
		lpCommandLine,
		lpCurrentDirectory);

	if (!ret) {
		return 0;
	}

	/* Ignore mspdbsrv.exe, since it continues to run in the background */
	if(!lpApplicationName || wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL
	   || wcscasestr(lpApplicationName, L"tup32detect.exe") == NULL) {
		tup_inject_dll(lpProcessInformation, s_depfilename, s_vardict_file);
	}

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

BOOL WINAPI CreateProcessWithTokenW_hook(
    __in        HANDLE hToken,
    __in        DWORD dwLogonFlags,
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL ret = CreateProcessWithTokenW_orig(
		hToken,
		dwLogonFlags,
		lpApplicationName,
		lpCommandLine,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation);

	DEBUG_HOOK("CreateProcessWithTokenW '%S' '%S' in '%S'\n",
		lpApplicationName,
		lpCommandLine,
		lpCurrentDirectory);

	if (!ret) {
		return 0;
	}

	/* Ignore mspdbsrv.exe, since it continues to run in the background */
	if(!lpApplicationName || wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL
	   || wcscasestr(lpApplicationName, L"tup32detect.exe") == NULL) {
		tup_inject_dll(lpProcessInformation, s_depfilename, s_vardict_file);
	}

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

int _access_hook(const char *pathname, int mode)
{
	handle_file(pathname, NULL, ACCESS_READ);
	return _access_orig(pathname, mode);
}

FILE *fopen_hook(const char *path, const char *mode)
{
	DEBUG_HOOK("fopen mode = %s\n", mode );

	FILE *ret = fopen_orig(path, mode);
	if(strchr(mode, 'w') == NULL &&
	   strchr(mode, 'a') == NULL &&
	   ( strchr(mode, '+') == NULL || ret == NULL ) ) {
		handle_file(path, NULL, ACCESS_READ);
	} else {
		handle_file(path, NULL, ACCESS_WRITE);
	}
	return ret;
}

int rename_hook(const char *oldpath, const char *newpath)
{
	handle_file(oldpath, newpath, ACCESS_RENAME);
	return rename_orig(oldpath, newpath);
}

int remove_hook(const char *pathname)
{
	handle_file(pathname, NULL, ACCESS_UNLINK);
	return remove_orig(pathname);
}

/* -------------------------------------------------------------------------- */


typedef HMODULE (WINAPI *LoadLibraryA_t)(const char*);
typedef FARPROC (WINAPI *GetProcAddress_t)(HMODULE, const char*);



struct remote_thread_t
{
	LoadLibraryA_t load_library;
	GetProcAddress_t get_proc_address;
	char depfilename[MAX_PATH];
	char vardict_file[MAX_PATH];
	char execdir[MAX_PATH];
	char dll_name[MAX_PATH];
	char func_name[256];
};

struct remote_thread32_t
{
	uint32_t load_library;
	uint32_t get_proc_address;
	char depfilename[MAX_PATH];
	char vardict_file[MAX_PATH];
	char execdir[MAX_PATH];
	char dll_name[MAX_PATH];
	char func_name[256];
}__attribute__((packed));




#define HOOK(name) { MODULE_NAME, #name, name##_hook, (void**)&name##_orig, 0 }
static struct patch_entry patch_table[] = {
#define MODULE_NAME "kernel32.dll"
	HOOK(OpenFile),
	HOOK(CreateFileA),
	HOOK(CreateFileW),
	HOOK(CreateFileTransactedA),
	HOOK(CreateFileTransactedW),
	HOOK(DeleteFileA),
	HOOK(DeleteFileW),
	HOOK(DeleteFileTransactedA),
	HOOK(DeleteFileTransactedW),
	HOOK(MoveFileA),
	HOOK(MoveFileW),
	HOOK(MoveFileExA),
	HOOK(MoveFileExW),
	HOOK(MoveFileWithProgressA),
	HOOK(MoveFileWithProgressW),
	HOOK(MoveFileTransactedA),
	HOOK(MoveFileTransactedW),
	HOOK(ReplaceFileA),
	HOOK(ReplaceFileW),
	HOOK(CopyFileA),
	HOOK(CopyFileW),
	HOOK(CopyFileExA),
	HOOK(CopyFileExW),
	HOOK(CopyFileTransactedA),
	HOOK(CopyFileTransactedW),
	HOOK(GetFileAttributesA),
	HOOK(GetFileAttributesW),
	HOOK(GetFileAttributesExA),
	HOOK(GetFileAttributesExW),
	HOOK(FindFirstFileA),
	HOOK(FindFirstFileW),
	HOOK(FindNextFileA),
	HOOK(FindNextFileW),
	HOOK(CreateProcessA),
	HOOK(CreateProcessW),
#undef MODULE_NAME
#define MODULE_NAME "advapi32.dll"
	HOOK(CreateProcessAsUserA),
	HOOK(CreateProcessAsUserW),
	HOOK(CreateProcessWithLogonW),
	HOOK(CreateProcessWithTokenW),
#undef MODULE_NAME
#define MODULE_NAME "ntdll.dll"
	HOOK(NtCreateFile),
	HOOK(NtOpenFile),
#undef MODULE_NAME
#define MODULE_NAME "msvcrt.dll"
	HOOK(_access),
	HOOK(fopen),
	HOOK(rename),
	HOOK(remove)
};
#undef HOOK
#undef MODULE_NAME
enum { patch_table_len = sizeof( patch_table ) / sizeof( patch_table[0] ) };


/* -------------------------------------------------------------------------- */

static char execdir[MAX_PATH];

void tup_inject_setexecdir(const char* dir)
{
	execdir[0] = '\0';
	strncat(execdir, dir, MAX_PATH);
	execdir[MAX_PATH - 1] = '\0';
}

/* -------------------------------------------------------------------------- */

static const char *strcasestr(const char *arg1, const char *arg2)
{
	const char *a, *b;

	for(;*arg1;arg1++) {

		a = arg1;
		b = arg2;

		while(tolower(*a++) == tolower(*b++)) {
			if(!*b) {
				return (arg1);
			}
		}

	}

	return(NULL);
}

static const wchar_t *wcscasestr(const wchar_t *arg1, const wchar_t *arg2)
{
	const wchar_t *a, *b;

	for(;*arg1;arg1++) {

		a = arg1;
		b = arg2;

		while(tolower(*a++) == tolower(*b++)) {
			if(!*b) {
				return (arg1);
			}
		}

	}

	return(NULL);
}

static int ignore_file(const char* file)
{
	if (!file)
		return 0;
	if (stricmp(file, "nul") == 0)
		return 1;
	if (stricmp(file, "nul:") == 0)
		return 1;
	if (stricmp(file, "prn") == 0)
		return 1;
	if (stricmp(file, "aux") == 0)
		return 1;
	if (stricmp(file, "con") == 0)
		return 1;
	if (strncmp(file, "com", 3) == 0 && isdigit(file[3]) && file[4] == '\0')
		return 1;
	if (strncmp(file, "lpt", 3) == 0 && isdigit(file[3]) && file[4] == '\0')
		return 1;
	if (strcasestr(file, "\\PIPE\\") != NULL)
		return 1;
	if (strnicmp(file, "PIPE\\", 5) == 0)
		return 1;
	if (strcasestr(file, "\\Device\\") != NULL)
		return 1;
	if (strstr(file, "$") != NULL)
		return 1;
	if (strncmp(file, "\\\\", 2) == 0)
		return 1;
	if (strcasestr(file, "SQM\\sqmcpp.log") != NULL)
		return 1;
	return 0;
}

static int ignore_file_w(const wchar_t* file)
{
	if (!file)
		return 0;
	if (wcsicmp(file, L"nul") == 0)
		return 1;
	if (wcsicmp(file, L"nul:") == 0)
		return 1;
	if (wcsicmp(file, L"prn") == 0)
		return 1;
	if (wcsicmp(file, L"aux") == 0)
		return 1;
	if (wcsicmp(file, L"con") == 0)
		return 1;
	if (wcsncmp(file, L"com", 3) == 0 && isdigit(file[3]) && file[4] == L'\0')
		return 1;
	if (wcsncmp(file, L"lpt", 3) == 0 && isdigit(file[3]) && file[4] == L'\0')
		return 1;
	if (wcscasestr(file, L"\\PIPE\\") != NULL)
		return 1;
	if (wcsstr(file, L"$") != NULL)
		return 1;
	if (wcsncmp(file, L"\\\\", 2) == 0)
		return 1;
	if (wcscasestr(file, L"SQM\\sqmcpp.log") != NULL)
		return 1;
	return 0;
}

static int canon_path(const char *file, char *dest)
{
	if(!file)
		return 0;
	if(is_full_path(file)) {
		/* Full path */
		PathCanonicalize(dest, file);
	} else {
		/* Relative path */
		char tmp[PATH_MAX];
		int cwdlen;
		int filelen = strlen(file);

		tmp[0] = 0;
		if(GetCurrentDirectory(sizeof(tmp), tmp) == 0) {
			/* TODO: Error handle? */
			return 0;
		}
		cwdlen = strlen(tmp);
		if(cwdlen + filelen + 2 >= (signed)sizeof(tmp)) {
			/* TODO: Error handle? */
			return 0;
		}
		tmp[cwdlen] = '\\';
		memcpy(tmp + cwdlen + 1, file, filelen + 1);
		PathCanonicalize(dest, tmp);
	}
	return strlen(dest);
}

static void mhandle_file(const char* file, const char* file2, enum access_type at, int line)
{
	DWORD save_error = GetLastError();

	char buf[ACCESS_EVENT_MAX_SIZE];
	struct access_event* e = (struct access_event*) buf;
	char* dest = (char*) (e + 1);
	int ret;
	if(line) {}

	if (ignore_file(file) || ignore_file(file2) || deph == INVALID_HANDLE_VALUE)
		goto exit;

	if(strncmp(file, "@tup@", 5) == 0) {
		const char *var = file+6;
		e->at = ACCESS_VAR;
		e->len = strlen(var);
		e->len2 = 0;
		strcpy(dest, var);
		dest += e->len;
		*(dest++) = '\0';
		*(dest++) = '\0';
	} else {
		e->at = at;

		e->len = canon_path(file, dest);
		DEBUG_HOOK("Canonicalize1 [%i]: '%s' -> '%s', len=%i\n", line, file, dest, e->len);
		dest += e->len;
		*(dest++) = '\0';

		e->len2 = canon_path(file2, dest);
		DEBUG_HOOK("Canonicalize2: '%s' -> '%s' len2=%i\n", file2, file2 ? dest : NULL, e->len2);
		dest += e->len2;
		*(dest++) = '\0';
	}

	DEBUG_HOOK("%s: '%s' '%s'\n", access_type_name[at], file, file2);
	ret = writef((char*) e, dest - (char*) e);
	DEBUG_HOOK("writef %d\n", ret);
	if(ret) {}

exit:
	SetLastError( save_error );
}

static void handle_file_w(const wchar_t* file, const wchar_t* file2, enum access_type at)
{
	DWORD save_error = GetLastError();

	char buf[ACCESS_EVENT_MAX_SIZE];
	char afile[PATH_MAX];
	char afile2[PATH_MAX];
	size_t fsz;
	size_t f2sz;
	struct access_event* e = (struct access_event*) buf;
	char* dest = (char*) (e + 1);
	int ret;
	int count;
	wchar_t backslash_prefix[] = L"\\\\?\\"; /* \\?\ can be used as a prefix in wide-char paths */
	const int backslash_prefix_len = 4;

	if (ignore_file_w(file) || ignore_file_w(file2) || deph == INVALID_HANDLE_VALUE)
		goto exit;

	if(file)
		if(wcsncmp(file, backslash_prefix, backslash_prefix_len) == 0)
			file += backslash_prefix_len;
	if(file2)
		if(wcsncmp(file2, backslash_prefix, backslash_prefix_len) == 0)
			file2 += backslash_prefix_len;

	fsz = file ? wcslen(file) : 0;
	f2sz = file2 ? wcslen(file2) : 0;

	e->at = at;

	count = WideCharToMultiByte(CP_UTF8, 0, file, fsz, afile, PATH_MAX, NULL, NULL);
	afile[count] = 0;
	count = WideCharToMultiByte(CP_UTF8, 0, file2, f2sz, afile2, PATH_MAX, NULL, NULL);
	afile2[count] = 0;

	e->len = canon_path(afile, dest);
	dest += e->len;
	*(dest++) = '\0';

	e->len2 = canon_path(afile2, dest);
	dest += e->len2;
	*(dest++) = '\0';

	DEBUG_HOOK("%s [wide, %i, %i]: '%S', '%S'\n", access_type_name[at], e->len, e->len2, file, file2);
	ret = writef((char*) e, dest - (char*) e);
	DEBUG_HOOK("writef [wide] %d\n", ret);
	if(ret) {}

exit:
	SetLastError( save_error );
}

static int open_file(const char *depfilename)
{
	deph = CreateFile(depfilename, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(deph == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "tup error: Unable to open dependency file '%s' in dllinject. Windows error code: 0x%08lx\n", depfilename, GetLastError());
		return -1;
	}
	return 0;
}

static int open_vardict_file(const char *vardict_file)
{
	vardicth = CreateFile(vardict_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(vardicth == INVALID_HANDLE_VALUE) {
		/* Not an error if the file doesn't exist - we may not have a vardict. */
		if(GetLastError() != ERROR_FILE_NOT_FOUND) {
			fprintf(stderr, "tup error: Unable to open vardict file '%s' in dllinject. Windows error code: 0x%08lx\n", vardict_file, GetLastError());
			return -1;
		}
	}
	return 0;
}

/* -------------------------------------------------------------------------- */

BOOL WINAPI DllMain(HANDLE HDllHandle, DWORD Reason, LPVOID Reserved)
{
	(void) HDllHandle;
	(void) Reason;
	(void) Reserved;
	return 1;
}

/* -------------------------------------------------------------------------- */

typedef DWORD (*tup_init_t)(remote_thread_t*);
DWORD tup_inject_init(remote_thread_t* r)
{
	static int initialised = 0;
	char filename[MAX_PATH];
	char vardict_env[64];
	int vardict_fd = -1;
	OSVERSIONINFO osinfo;

	if (initialised)
		return 0;

	initialised = 1;

	if (!GetModuleFileNameA(NULL, filename, sizeof(filename))) {
		return 1;
	}

	DEBUG_HOOK("Inside tup_dllinject_init '%s' '%s' '%s' '%s' '%s'\n",
		filename,
		r->execdir,
		r->dll_name,
		r->func_name,
		r->depfilename);

	DEBUG_HOOK(" - injected into %d: %s\n", GetCurrentProcessId(), GetCommandLineA());

	tup_inject_setexecdir(r->execdir);

	if (open_file(r->depfilename))
		return 1;
	if (open_vardict_file(r->vardict_file))
		return 1;

	if(vardicth != INVALID_HANDLE_VALUE) {
		vardict_fd = _open_osfhandle((intptr_t)vardicth, 0);
	}
	snprintf(vardict_env, sizeof(vardict_env), TUP_VARDICT_NAME "=%i", vardict_fd);
	vardict_env[sizeof(vardict_env)-1] = 0;
	putenv(vardict_env);

	strcpy(s_depfilename, r->depfilename);
	strcpy(s_vardict_file, r->vardict_file);

	handle_file(filename, NULL, ACCESS_READ);

	/* What a horrible API... */
	osinfo.dwOSVersionInfoSize = sizeof(osinfo);
	GetVersionEx(&osinfo);

	if(osinfo.dwMajorVersion >= 6) {
		/* Only hot patch for Windows Vista and above. Hot patching
		 * here gets our hook for FindFirstFile, which iat patching
		 * doesn't get for some reason. I also tried to just iat patch
		 * NtQueryDirectoryFile(), but then that ends up crashing for
		 * some reason.
		 *
		 * For XP, the FindFirstFile hook works with iat patching, but
		 * hot patching breaks file removal for some reason, so for
		 * example 'gcc -flto foo.o -o foo.exe' will fail.
		 */
		hot_patch( patch_table, patch_table + patch_table_len );
	}
	iat_patch( patch_table, patch_table + patch_table_len );

	return 0;
}

#ifdef _WIN64
int remote_stub(void);
__asm(
  ".globl remote_stub\n"
  "remote_stub:\n"
  "subq $8, %rsp\n"
  "movl $0x556677, (%rsp)\n"		// return address, [0x7]
  "movl $0x11223344, 4(%rsp)\n"		// return address, [0xf]
  "pushf\n"
  "push %r15\n"
  "push %r14\n"
  "push %r13\n"
  "push %r12\n"
  "push %r11\n"
  "push %r10\n"
  "push %r9\n"
  "push %r8\n"
  "push %rbp\n"
  "push %rdi\n"
  "push %rsi\n"
  "push %rdx\n"
  "push %rcx\n"
  "push %rbx\n"
  "push %rax\n"
  "xorq %rcx, %rcx\n"
  "movq $0x1100000055667788, %rcx\n"	// function parameter [0x30]
  "xorq %rax, %rax\n"
  "movq $0x9900000055667788, %rax\n" 	// function to call, [0x3d]
  "call *%rax\n"
  "pop %rax\n"
  "pop %rbx\n"
  "pop %rcx\n"
  "pop %rdx\n"
  "pop %rsi\n"
  "pop %rdi\n"
  "pop %rbp\n"
  "pop %r8\n"
  "pop %r9\n"
  "pop %r10\n"
  "pop %r11\n"
  "pop %r12\n"
  "pop %r13\n"
  "pop %r14\n"
  "pop %r15\n"
  "popf\n"
  "ret"
);

static void WINAPI remote_init( remote_thread_t *r )
{
	HMODULE h;
	tup_init_t p;
	h = r->load_library(r->dll_name);
	if (!h)
		return;

	p = (tup_init_t) r->get_proc_address(h, r->func_name);
	if (!p)
		return;

	p(r);
}

static void remote_end(void)
{
}
#endif


#if __DBG_W64 == 1
static void printHex(const void *lpvbits, const unsigned int n)
{
    char* data = (char*) lpvbits;
    unsigned int i = 0;
    char line[17] = {};
    printf("%.8X | ", (unsigned char*)data);
    while ( i < n ) {
        line[i%16] = *(data+i);
        if ((line[i%16] < 32) || (line[i%16] > 126)) {
            line[i%16] = '.';
        }
        printf("%.2X", (unsigned char)*(data+i));
        i++;
        if (i%4 == 0) {
            if (i%16 == 0) {
                if (i < n-1)
                    printf(" | %s\n%.8X | ", &line, data+i);
            } else {
                printf(" ");
            }
        }
    }
    while (i%16 > 0) {
        (i%4 == 0)?printf("   "):printf("  ");
        line[i%16] = ' ';
        i++;
    }
    printf(" | %s\n", &line);
}
#endif

inline long long unsigned int low32(long long unsigned int tall)
{
        return tall & 0x00000000ffffffff;
}
inline long long unsigned int high32(long long unsigned int tall)
{
        return tall >> 32;
}

struct remote_stub_t {
	uint8_t stub[23];
	uint8_t fileA_Hook[39];
	uint8_t fileW_Hook[39];
	uint8_t remote_init[60];
}__attribute__((packed));

static struct remote_stub_t remote_stub32 = {
	.stub = {
0x68, 0x00, 0x00, 0x00, 0x00,
0x9c,
0x60,
0x68, 0xef, 0xbe, 0xad, 0xde,
0xb8, 0xef, 0xbe, 0xad, 0xde,
0xff, 0xd0,
0x61,
0x9d,
0xc3
},
	.fileA_Hook = {
0x55,
0x89, 0xe5,
0x83, 0xec, 0x18,
0x8b, 0x45, 0x0c,
0x89, 0x44, 0x24, 0x04,
0x8b, 0x45, 0x08,
0x89, 0x04, 0x24,
0xff, 0x15, 0x78, 0x00, 0x00, 0x00,
0x85, 0xc0,
0x52,
0x0f, 0x95, 0xc0,
0x52,
0x0f, 0xb6, 0xc0,
0xc9,
0xc2, 0x08, 0x00
},

	.fileW_Hook = {
0x55,
0x89, 0xe5,
0x83, 0xec, 0x18,
0x8b, 0x45, 0x0c,
0x89, 0x44, 0x24, 0x04,
0x8b, 0x45, 0x08,
0x89, 0x04, 0x24,
0xff, 0x15, 0x7c, 0x00, 0x00, 0x00,
0x85, 0xc0,
0x51,
0x0f, 0x95, 0xc0,
0x51,
0x0f, 0xb6, 0xc0,
0xc9,
0xc2, 0x08, 0x00
},

	.remote_init = {
0x55,
0x89, 0xe5,
0x53,
0x83, 0xec, 0x14,
0x8b, 0x5d, 0x08,
0x8d, 0x83, 0x14, 0x03, 0x00, 0x00,
0x89, 0x04, 0x24,
0xff, 0x13,
0x85, 0xc0,
0x51,
0x74, 0x1b,				// JE 0x1b
0x8d, 0x93, 0x18, 0x04, 0x00, 0x00,
0x89, 0x54, 0x24, 0x04,
0x89, 0x04, 0x24,
0xff, 0x53, 0x04,
0x85, 0xc0,
0x52,
0x52,
0x74, 0x05,				// JE 0x05
0x89, 0x1c, 0x24,
0xff, 0xd0,
0x8b, 0x5d, 0xfc,
0xc9,
0xc2, 0x04, 0x00}
};

static uint32_t LOAD_LIBRARY_32 = 0;
static uint32_t GET_PROC_ADDRESS_32 = 0;

#define BUFSIZE 4096

BOOL get_wow64_addresses(void)
{
	DWORD dwRead;
	CHAR chBuf[BUFSIZE];
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO  siStartInfo;
	BOOL ret;

	TCHAR szCmdline[]=TEXT("tup32detect.exe");

	HANDLE g_hChildStd_OUT_Rd = NULL;
	HANDLE g_hChildStd_OUT_Wr = NULL;

	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Pipe stdout
	if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) )
		return FALSE;

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
		return FALSE;

	// create process
	memset(&siStartInfo, 0, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	memset(&piProcInfo, 0, sizeof(PROCESS_INFORMATION));

	// Detect and avoid inception!
	if (CreateProcessA_orig != NULL)
		ret = CreateProcessA_orig(
				NULL,
				szCmdline,
				NULL,
				NULL,
				TRUE,
				0,
				NULL,
				NULL,
				&siStartInfo,
				&piProcInfo);
	else
		ret = CreateProcessA(
				NULL,
				szCmdline,
				NULL,
				NULL,
				TRUE,
				0,
				NULL,
				NULL,
				&siStartInfo,
				&piProcInfo);

	if (!ret) {
		DEBUG_HOOK("Unable to spawn tup32detect.exe\n");
		return FALSE;
	}

	ret = ReadFile( g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
	if (!ret || dwRead == 0)
		return FALSE;

	if (sscanf(chBuf, "%x-%x", &LOAD_LIBRARY_32, &GET_PROC_ADDRESS_32) != 2)
		return FALSE;

	DEBUG_HOOK("Got addresses: %x, %x\n", LOAD_LIBRARY_32, GET_PROC_ADDRESS_32);
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	return TRUE;
}



int tup_inject_dll(
	LPPROCESS_INFORMATION lpProcessInformation,
	const char *depfilename,
	const char *vardict_file)
{
	char* remote_data;
	size_t code_size;
	DWORD old_protect;
	HANDLE process;
	BOOL bWow64 = 0;

	IsWow64Process(lpProcessInformation->hProcess, &bWow64);

	// WOW64
	DEBUG_HOOK("%s is WOW64: %i\n", GetCommandLineA(), bWow64);
	if (bWow64) {
		remote_thread32_t remote;

		if (GET_PROC_ADDRESS_32 == 0) {
			if ( ! get_wow64_addresses() ) {
				printf("Unable to retrieve WOW64 info\n");
				return -1;
			}
		}

		memset(&remote, 0, sizeof(remote));
		remote.load_library = LOAD_LIBRARY_32;
		remote.get_proc_address = GET_PROC_ADDRESS_32;
		strcpy(remote.depfilename, depfilename);
		strcpy(remote.vardict_file, vardict_file);
		strcat(remote.execdir, execdir);
		strcat(remote.dll_name, execdir);
		strcat(remote.dll_name, "\\");
		strcat(remote.dll_name, "tup-dllinject32.dll");
		strcat(remote.func_name, "tup_inject_init");

		WOW64_CONTEXT ctx;
		ctx.ContextFlags = WOW64_CONTEXT_CONTROL;
		if ( !Wow64GetThreadContext( lpProcessInformation->hThread, &ctx ) )
			return -1;

		/* Align code_size to a 16 byte boundary */
		code_size = (sizeof(remote_stub32) + 0x0F) & ~0x0F;

		DEBUG_HOOK("Injecting dll '%s' '%s' %s' '%s'\n",
			remote.execdir,
			remote.dll_name,
			remote.func_name,
			remote.depfilename,
			remote.vardict_file);

		process = lpProcessInformation->hProcess;

		if (!WaitForInputIdle(process, INFINITE))
			return -1;

		remote_data = (char*) VirtualAllocEx(
			process,
			NULL,
			code_size + sizeof(remote),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE);

		if (!remote_data)
			return -1;

		if (!VirtualProtectEx(process, remote_data, code_size + sizeof(remote), PAGE_READWRITE, &old_protect))
			return -1;

		unsigned char code[code_size];

		memcpy( code, &remote_stub32, code_size );

		*(DWORD*)(code + 0x1) = ctx.Eip;											// Return addr
		*(DWORD*)(code + 0x8) = (DWORD)((DWORD_PTR)remote_data + code_size);							// Arg (ptr to remote (TCB))
		*(DWORD*)(code + 0xd) = (DWORD)((DWORD_PTR)remote_data + ((DWORD_PTR)&remote_stub32.remote_init - (DWORD_PTR)&remote_stub32));	// Func (ptr to remote_init)

		if (!WriteProcessMemory(process, remote_data, code, code_size, NULL))
			return -1;

		if (!WriteProcessMemory(process, remote_data + code_size, &remote, sizeof(remote), NULL))
			return -1;

		if (!VirtualProtectEx(process, remote_data, code_size + sizeof(remote), PAGE_EXECUTE_READ, &old_protect))
			return -1;

		if (!FlushInstructionCache(process, remote_data, code_size + sizeof(remote)))
			return -1;

		ctx.Eip = (DWORD_PTR)remote_data;
		ctx.ContextFlags = WOW64_CONTEXT_CONTROL;
		if( !Wow64SetThreadContext( lpProcessInformation->hThread, &ctx ) )
			return -1;
	} else {
#ifdef _WIN64
		HMODULE kernel32;
		remote_thread_t remote;

		memset(&remote, 0, sizeof(remote));
		kernel32 = LoadLibraryA("kernel32.dll");
		remote.load_library = (LoadLibraryA_t) GetProcAddress(kernel32, "LoadLibraryA");
		remote.get_proc_address = (GetProcAddress_t) GetProcAddress(kernel32, "GetProcAddress");
		strcpy(remote.depfilename, depfilename);
		strcpy(remote.vardict_file, vardict_file);
		strcat(remote.execdir, execdir);
		strcat(remote.dll_name, execdir);
		strcat(remote.dll_name, "\\");
		strcat(remote.dll_name, "tup-dllinject.dll");
		strcat(remote.func_name, "tup_inject_init");

		CONTEXT ctx;
		ctx.ContextFlags = CONTEXT_CONTROL;
		if( !GetThreadContext( lpProcessInformation->hThread, &ctx ) )
			return -1;

		/* Align code_size to a 16 byte boundary */
		code_size = (  (uintptr_t) &remote_end
			     - (uintptr_t) &remote_stub + 0x0F)
			  & ~0x0F;


		DEBUG_HOOK("Injecting dll '%s' '%s' %s' '%s'\n",
			remote.execdir,
			remote.dll_name,
			remote.func_name,
			remote.depfilename,
			remote.vardict_file);

		process = lpProcessInformation->hProcess;

		if (!WaitForInputIdle(process, INFINITE))
			return -1;

		remote_data = (char*) VirtualAllocEx(
			process,
			NULL,
			code_size + sizeof(remote),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE);

		if (!remote_data)
			return -1;

		if (!VirtualProtectEx(process, remote_data, code_size + sizeof(remote), PAGE_READWRITE, &old_protect))
			return -1;

		unsigned char code[code_size];

		memcpy( code, &remote_stub, code_size );
		*(DWORD*)(code + 0x7) = low32(ctx.Rip);
		*(DWORD*)(code + 0xf) = high32(ctx.Rip);
		*(DWORD64*)(code + 0x30) = (long long unsigned int)(remote_data + code_size);
		*(DWORD64*)(code + 0x3d) = (long long unsigned int)(DWORD_PTR)remote_data + ((DWORD_PTR)&remote_init - (DWORD_PTR)&remote_stub);

		if (!WriteProcessMemory(process, remote_data, code, code_size, NULL))
			return -1;

		if (!WriteProcessMemory(process, remote_data + code_size, &remote, sizeof(remote), NULL))
			return -1;

		if (!VirtualProtectEx(process, remote_data, code_size + sizeof(remote), PAGE_EXECUTE_READ, &old_protect))
			return -1;

		if (!FlushInstructionCache(process, remote_data, code_size + sizeof(remote)))
			return -1;

		ctx.Rip = (DWORD_PTR)remote_data;
		ctx.ContextFlags = CONTEXT_CONTROL;
		if( !SetThreadContext( lpProcessInformation->hThread, &ctx ) )
			return -1;
#else
		DEBUG_HOOK("Error: Shouldn't be hooking here for the 32-bit dll.\n");
		return -1;
#endif
	}

	return 0;
}
