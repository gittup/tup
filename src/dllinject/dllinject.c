/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010  James McKaskill
 * Copyright (C) 2010-2012  Mike Shal <marfey@gmail.com>
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

#include <windows.h>
#include <ntdef.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <ctype.h>
#include <shlwapi.h>

#ifndef __in
#define __in
#define __out
#define __inout
#define __in_opt
#define __inout_opt
#define __reserved
#endif

#ifndef NDEBUG
#	define DEBUG_HOOK debug_hook

static const char* access_type_name[] = {
	"read",
	"write",
	"rename",
	"unlink",
	"var",
};

FILE *debugf = NULL;
int opening = 0;
static void debug_hook(const char* format, ...)
{
	char buf[256];
	va_list ap;
	if(debugf == NULL && !opening) {
		opening = 1;
		debugf = fopen("c:\\cygwin\\home\\marf\\ok.txt", "a");
		fflush(stdout);
	}
	if(debugf == NULL) {
		printf("No file :(\n");
		return;
	}
	va_start(ap, format);
	vsnprintf(buf, 255, format, ap);
	buf[255] = '\0';
	fprintf(debugf, buf);
	fflush(debugf);
}
#else
#	define DEBUG_HOOK(...)
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

#define handle_file(a, b, c) mhandle_file(a, b, c, __LINE__)
static void mhandle_file(const char* file, const char* file2, enum access_type at, int line);
static void handle_file_w(const wchar_t* file, const wchar_t* file2, enum access_type at);

static const char *strcasestr(const char *arg1, const char *arg2);
static const wchar_t *wcscasestr(const wchar_t *arg1, const wchar_t *arg2);

static char s_depfilename[PATH_MAX];
static HANDLE deph = INVALID_HANDLE_VALUE;

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

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & GENERIC_WRITE) {
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

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & GENERIC_WRITE) {
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

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & GENERIC_WRITE) {
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

	if (h != INVALID_HANDLE_VALUE && dwDesiredAccess & GENERIC_WRITE) {
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

		if (rc == STATUS_SUCCESS && DesiredAccess & GENERIC_WRITE) {
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
			if (rc == STATUS_SUCCESS && DesiredAccess & GENERIC_WRITE) {
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
	return FindFirstFileA_orig(lpFileName, lpFindFileData);
}

__out HANDLE WINAPI FindFirstFileW_hook(
    __in  LPCWSTR lpFileName,
    __out LPWIN32_FIND_DATAW lpFindFileData)
{
	DEBUG_HOOK("FindFirstFileW '%S'\n", lpFileName);
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
	if(strcasestr(lpApplicationName, "mspdbsrv.exe") == NULL)
		tup_inject_dll(lpProcessInformation, s_depfilename);

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
	if(wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL)
		tup_inject_dll(lpProcessInformation, s_depfilename);

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
	if(strcasestr(lpApplicationName, "mspdbsrv.exe") == NULL)
		tup_inject_dll(lpProcessInformation, s_depfilename);

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
	if(wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL)
		tup_inject_dll(lpProcessInformation, s_depfilename);

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
	if(wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL)
		tup_inject_dll(lpProcessInformation, s_depfilename);

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
	if(wcscasestr(lpApplicationName, L"mspdbsrv.exe") == NULL)
		tup_inject_dll(lpProcessInformation, s_depfilename);

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
	char execdir[MAX_PATH];
	char dll_name[MAX_PATH];
	char func_name[256];
};


typedef void (*foreach_import_t)(HMODULE, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur);
static void foreach_module(HMODULE h, foreach_import_t kernel32, foreach_import_t advapi32, foreach_import_t nt, foreach_import_t msvcrt)
{
	IMAGE_DOS_HEADER* dos_header;
	IMAGE_NT_HEADERS* nt_headers;
	IMAGE_DATA_DIRECTORY* import_dir;
	IMAGE_IMPORT_DESCRIPTOR* imports;

	dos_header = (IMAGE_DOS_HEADER*) h;
	nt_headers = (IMAGE_NT_HEADERS*) (dos_header->e_lfanew + (char*) h);

	import_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	imports = (IMAGE_IMPORT_DESCRIPTOR*) (import_dir->VirtualAddress + (char*) h);
	if (import_dir->VirtualAddress == 0)
		return;

	while (imports->Name != 0) {
		char* dllname = (char*) h + imports->Name;
		if (imports->FirstThunk && imports->OriginalFirstThunk) {
			IMAGE_THUNK_DATA* cur = (IMAGE_THUNK_DATA*) (imports->FirstThunk + (char*) h);
			IMAGE_THUNK_DATA* orig = (IMAGE_THUNK_DATA*) (imports->OriginalFirstThunk + (char*) h);
			if (stricmp(dllname, "kernel32.dll") == 0) {
				while (cur->u1.Function && orig->u1.Function) {
					kernel32(h, orig, cur);
					cur++;
					orig++;
				}
			} else if (stricmp(dllname, "advapi32.dll") == 0) {
				while (cur->u1.Function && orig->u1.Function) {
					advapi32(h, orig, cur);
					cur++;
					orig++;
				}
			} else if (stricmp(dllname, "ntdll.dll") == 0) {
				while (cur->u1.Function && orig->u1.Function) {
					nt(h, orig, cur);
					cur++;
					orig++;
				}
			} else if(stricmp(dllname, "msvcrt.dll") == 0) {
				while (cur->u1.Function && orig->u1.Function) {
					msvcrt(h, orig, cur);
					cur++;
					orig++;
				}
			}
		}
		imports++;
	}
}

static void do_hook(void* fphook, void** fporig, IMAGE_THUNK_DATA* cur)
{
	DWORD old_protect;
	*fporig = (void*) cur->u1.Function;
	if (!VirtualProtect(cur, sizeof(IMAGE_THUNK_DATA), PAGE_EXECUTE_READWRITE, &old_protect)) {
		return;
	}

	cur->u1.Function = (uintptr_t) fphook;

	if (!VirtualProtect(cur, sizeof(IMAGE_THUNK_DATA), old_protect, &old_protect)) {
		return;
	}
}

static void hook(HMODULE h, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur, void* fphook, void** fporig, const char* wanted_name, DWORD wanted_ordinal)
{
	if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
		DWORD ordinal = orig->u1.Ordinal & ~IMAGE_ORDINAL_FLAG;
		if (ordinal == wanted_ordinal) {
			do_hook(fphook, fporig, cur);
		}
	} else {
		IMAGE_IMPORT_BY_NAME* name = (IMAGE_IMPORT_BY_NAME*) (orig->u1.AddressOfData + (char*) h);
		if (strcmp((const char*) name->Name, wanted_name) == 0) {
			do_hook(fphook, fporig, cur);
		}
	}
}

#define HOOK_ORD(name, ordinal) hook(h, orig, cur, (void*) name##_hook, (void**) &name##_orig, #name, ordinal)
#define HOOK(name) hook(h, orig, cur, (void*) name##_hook, (void**) &name##_orig, #name, IMAGE_ORDINAL_FLAG)

static void have_kernel32_import(HMODULE h, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur)
{
	HOOK(OpenFile);
	HOOK(CreateFileA);
	HOOK(CreateFileW);
	HOOK(CreateFileTransactedA);
	HOOK(CreateFileTransactedW);
	HOOK(DeleteFileA);
	HOOK(DeleteFileW);
	HOOK(DeleteFileTransactedA);
	HOOK(DeleteFileTransactedW);
	HOOK(MoveFileA);
	HOOK(MoveFileW);
	HOOK(MoveFileExA);
	HOOK(MoveFileExW);
	HOOK(MoveFileWithProgressA);
	HOOK(MoveFileWithProgressW);
	HOOK(MoveFileTransactedA);
	HOOK(MoveFileTransactedW);
	HOOK(ReplaceFileA);
	HOOK(ReplaceFileW);
	HOOK(CopyFileA);
	HOOK(CopyFileW);
	HOOK(CopyFileExA);
	HOOK(CopyFileExW);
	HOOK(CopyFileTransactedA);
	HOOK(CopyFileTransactedW);
	HOOK(GetFileAttributesA);
	HOOK(GetFileAttributesW);
	HOOK(GetFileAttributesExA);
	HOOK(GetFileAttributesExW);
	HOOK(FindFirstFileA);
	HOOK(FindFirstFileW);
	HOOK(FindNextFileA);
	HOOK(FindNextFileW);
	HOOK(CreateProcessA);
	HOOK(CreateProcessW);
}

static void have_advapi32_import(HMODULE h, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur)
{
	HOOK(CreateProcessAsUserA);
	HOOK(CreateProcessAsUserW);
	HOOK(CreateProcessWithLogonW);
	HOOK(CreateProcessWithTokenW);
}

static void have_nt_import(HMODULE h, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur)
{
	HOOK(NtCreateFile);
	HOOK(NtOpenFile);
}

static void have_msvcrt_import(HMODULE h, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur)
{
	HOOK(_access);
	HOOK(fopen);
	HOOK(rename);
	HOOK(remove);
}

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
	return 0;
}

static int ignore_file_w(const wchar_t* file)
{
	if (!file)
		return 0;
	if (wcsicmp(file, L"nul") == 0)
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
	char buf[ACCESS_EVENT_MAX_SIZE];
	struct access_event* e = (struct access_event*) buf;
	char* dest = (char*) (e + 1);
	int ret;
	if(line) {}

	if (ignore_file(file) || ignore_file(file2) || deph == INVALID_HANDLE_VALUE)
		return;

	e->at = at;

	e->len = canon_path(file, dest);
	DEBUG_HOOK("Canonicalize1 [%i]: '%s' -> '%s', len=%i\n", line, file, dest, e->len);
	dest += e->len;
	*(dest++) = '\0';

	e->len2 = canon_path(file2, dest);
	DEBUG_HOOK("Canonicalize2: '%s' -> '%s' len2=%i\n", file2, file2 ? dest : NULL, e->len2);
	dest += e->len2;
	*(dest++) = '\0';

	DEBUG_HOOK("%s: '%s' '%s'\n", access_type_name[at], file, file2);
	ret = writef((char*) e, dest - (char*) e);
	DEBUG_HOOK("writef %d\n", ret);
	if(ret) {}
}

static void handle_file_w(const wchar_t* file, const wchar_t* file2, enum access_type at)
{
	char buf[ACCESS_EVENT_MAX_SIZE];
	char afile[PATH_MAX];
	char afile2[PATH_MAX];
	size_t fsz = file ? wcslen(file) : 0;
	size_t f2sz = file2 ? wcslen(file2) : 0;
	struct access_event* e = (struct access_event*) buf;
	char* dest = (char*) (e + 1);
	int ret;
	int count;

	if (ignore_file_w(file) || ignore_file_w(file2) || deph == INVALID_HANDLE_VALUE)
		return;

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
}

static int open_file(const char *depfilename)
{
	deph = CreateFile(depfilename, FILE_APPEND_DATA, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(deph == INVALID_HANDLE_VALUE) {
		perror(depfilename);
		return -1;
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

	size_t i;
	DWORD modnum;
	HMODULE modules[256];
	char filename[MAX_PATH];

	if (initialised)
		return 0;

	/* Put TUP_VARDICT_NAME in the environment so if tup is running as the
	 * sub-process it knows that certain commands are unavailable. Note
	 * this isn't actually a valid file id, so varsed and all will fail.
	 */
	putenv(TUP_VARDICT_NAME "=-1");

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

	DEBUG_HOOK("%d: %s\n", GetCurrentProcessId(), GetCommandLineA());

	if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &modnum)) {
		return 1;
	}

	modnum /= sizeof(HMODULE);

	tup_inject_setexecdir(r->execdir);

	if (open_file(r->depfilename))
		return 1;

	strcpy(s_depfilename, r->depfilename);

	handle_file(filename, NULL, ACCESS_READ);

	for (i = 0; i < modnum; i++) {
		if (!GetModuleFileNameA(modules[i], filename, sizeof(filename))) {
			return 1;
		}
		handle_file(filename, NULL, ACCESS_READ);

		foreach_module(modules[i], &have_kernel32_import, &have_advapi32_import, &have_nt_import, &have_msvcrt_import);
	}

	return 0;
}

int remote_stub(void);
__asm(
  ".globl _remote_stub\n"
  "_remote_stub:\n"
  "pushl $0xDEADBEEF\n"    // return address, [1]
  "pushfl\n"
  "pushal\n"
  "pushl $0xDEADBEEF\n"    // function parameter, [8]
  "movl $0xDEADBEEF, %eax\n" // function to call, [13]
  "call *%eax\n"
  "popal\n"
  "popfl\n"
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

int tup_inject_dll(
	LPPROCESS_INFORMATION lpProcessInformation,
	const char *depfilename)
{
	remote_thread_t remote;
	char* remote_data;
	size_t code_size;
	DWORD old_protect;
	HANDLE process;
	HMODULE kernel32;

	memset(&remote, 0, sizeof(remote));
	kernel32 = LoadLibraryA("kernel32.dll");
	remote.load_library = (LoadLibraryA_t) GetProcAddress(kernel32, "LoadLibraryA");
	remote.get_proc_address = (GetProcAddress_t) GetProcAddress(kernel32, "GetProcAddress");
	strcpy(remote.depfilename, depfilename);
	strcat(remote.execdir, execdir);
	strcat(remote.dll_name, execdir);
	strcat(remote.dll_name, "\\");
	strcat(remote.dll_name, "tup-dllinject.dll");
	strcat(remote.func_name, "tup_inject_init");

	CONTEXT ctx;
	ctx.ContextFlags = CONTEXT_CONTROL;
	if( !GetThreadContext( lpProcessInformation->hThread, &ctx ) )
		return -1;

	DEBUG_HOOK("Injecting dll '%s' '%s' %s' '%s'\n",
		remote.execdir,
		remote.dll_name,
		remote.func_name,
		remote.depfilename);

	process = lpProcessInformation->hProcess;

	if (!WaitForInputIdle(process, INFINITE))
		return -1;

	/* Align code_size to a 16 byte boundary */
	code_size = (  (uintptr_t) &remote_end
		     - (uintptr_t) &remote_stub + 0x0F)
		  & ~0x0F;

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
	*(DWORD*)(code + 1) = ctx.Eip;
	*(DWORD*)(code + 8) = (DWORD)remote_data + code_size;
	*(DWORD*)(code + 13) = (DWORD)remote_data + ( (DWORD)&remote_init - (DWORD)&remote_stub );
	if (!WriteProcessMemory(process, remote_data, code, code_size, NULL))
		return -1;

	if (!WriteProcessMemory(process, remote_data + code_size, &remote, sizeof(remote), NULL))
		return -1;

	if (!VirtualProtectEx(process, remote_data, code_size + sizeof(remote), PAGE_EXECUTE_READ, &old_protect))
		return -1;

	if (!FlushInstructionCache(process, remote_data, code_size + sizeof(remote)))
		return -1;

	ctx.Eip = (DWORD)remote_data;
	ctx.ContextFlags = CONTEXT_CONTROL;
	if( !SetThreadContext( lpProcessInformation->hThread, &ctx ) )
        return -1;

	return 0;
}
