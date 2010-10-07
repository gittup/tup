#define BUILDING_DLLINJECT
#include "dllinject.h"
#include "tup/access_event.h"

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <ctype.h>

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
	"symlink",
	"ghost",
};

static void debug_hook(const char* format, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, 255, format, ap);
	buf[255] = '\0';
	OutputDebugStringA(buf);
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


static void handle_file(const char* file, const char* file2, enum access_type at);
static void handle_file_w(const wchar_t* file, const wchar_t* file2, enum access_type at);

static unsigned short s_udp_port;

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
	} else if (uStyle & (OF_PARSE | OF_VERIFY)) {
		handle_file(lpFileName, NULL, ACCESS_GHOST);
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

	if (h == INVALID_HANDLE_VALUE) {
		handle_file(lpFileName, NULL, ACCESS_GHOST);
	} else if (dwDesiredAccess & GENERIC_WRITE) {
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

	if (h == INVALID_HANDLE_VALUE) {
		handle_file_w(lpFileName, NULL, ACCESS_GHOST);
	} else if (dwDesiredAccess & GENERIC_WRITE) {
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

	if (h == INVALID_HANDLE_VALUE) {
		handle_file(lpFileName, NULL, ACCESS_GHOST);
	} else if (dwDesiredAccess & GENERIC_WRITE) {
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

	if (h == INVALID_HANDLE_VALUE) {
		handle_file_w(lpFileName, NULL, ACCESS_GHOST);
	} else if (dwDesiredAccess & GENERIC_WRITE) {
		handle_file_w(lpFileName, NULL, ACCESS_WRITE);
	} else {
		handle_file_w(lpFileName, NULL, ACCESS_READ);
	}

	return h;
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

DWORD WINAPI GetFileAttributesA_hook(
    __in LPCSTR lpFileName)
{
	DEBUG_HOOK("GetFileAttributesA '%s'\n", lpFileName);
	handle_file(lpFileName, NULL, ACCESS_GHOST);
	return GetFileAttributesA_orig(lpFileName);
}

DWORD WINAPI GetFileAttributesW_hook(
    __in LPCWSTR lpFileName)
{
	handle_file_w(lpFileName, NULL, ACCESS_GHOST);
	return GetFileAttributesW_orig(lpFileName);
}

BOOL WINAPI GetFileAttributesExA_hook(
    __in  LPCSTR lpFileName,
    __in  GET_FILEEX_INFO_LEVELS fInfoLevelId,
    __out LPVOID lpFileInformation)
{
	handle_file(lpFileName, NULL, ACCESS_GHOST);
	return GetFileAttributesExA_orig(
		lpFileName,
		fInfoLevelId,
		lpFileInformation);
}

BOOL WINAPI GetFileAttributesExW_hook(
    __in  LPCWSTR lpFileName,
    __in  GET_FILEEX_INFO_LEVELS fInfoLevelId,
    __out LPVOID lpFileInformation)
{
	handle_file_w(lpFileName, NULL, ACCESS_GHOST);
	return GetFileAttributesExW_orig(
		lpFileName,
		fInfoLevelId,
		lpFileInformation);
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

	tup_inject_dll(lpProcessInformation, s_udp_port);

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

	tup_inject_dll(lpProcessInformation, s_udp_port);

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

	tup_inject_dll(lpProcessInformation, s_udp_port);

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

	tup_inject_dll(lpProcessInformation, s_udp_port);

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

	tup_inject_dll(lpProcessInformation, s_udp_port);

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

	tup_inject_dll(lpProcessInformation, s_udp_port);

	if ((dwCreationFlags & CREATE_SUSPENDED) != 0)
		return 1;

	return ResumeThread(lpProcessInformation->hThread) != 0xFFFFFFFF;
}

/* -------------------------------------------------------------------------- */


typedef HMODULE (WINAPI *LoadLibraryA_t)(const char*);
typedef FARPROC (WINAPI *GetProcAddress_t)(HMODULE, const char*);

struct remote_thread_t
{
	LoadLibraryA_t load_library;
	GetProcAddress_t get_proc_address;
	unsigned short udp_port;
	char execdir[MAX_PATH];
	char dll_name[MAX_PATH];
	char func_name[256];
};


typedef void (*foreach_import_t)(HMODULE, IMAGE_THUNK_DATA* orig, IMAGE_THUNK_DATA* cur);
static void foreach_module(HMODULE h, foreach_import_t kernel32, foreach_import_t advapi32)
{
	IMAGE_DOS_HEADER* dos_header;
	IMAGE_NT_HEADERS* nt_headers;
	IMAGE_SECTION_HEADER* section_headers;
	IMAGE_DATA_DIRECTORY* import_dir;
	IMAGE_IMPORT_DESCRIPTOR* imports;

	dos_header = (IMAGE_DOS_HEADER*) h;
	nt_headers = (IMAGE_NT_HEADERS*) (dos_header->e_lfanew + (char*) h);
	section_headers = (IMAGE_SECTION_HEADER*) (nt_headers + 1);

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

/* -------------------------------------------------------------------------- */

static char execdir[MAX_PATH];

void tup_inject_setexecdir(const char* dir)
{
	execdir[0] = '\0';
	strncat(execdir, dir, MAX_PATH);
	execdir[MAX_PATH - 1] = '\0';
}

/* -------------------------------------------------------------------------- */

static SOCKET sock = INVALID_SOCKET;

const char *strcasestr(const char *arg1, const char *arg2)
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

const wchar_t *wcscasestr(const wchar_t *arg1, const wchar_t *arg2)
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
	if (strcasestr(file, "\\temp\\") != NULL)
		return 1;
	if (strcasestr(file, "\\temp/") != NULL)
		return 1;
	if (strcasestr(file, "/temp\\") != NULL)
		return 1;
	if (strcasestr(file, "\\PIPE\\") != NULL)
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
	if (wcscasestr(file, L"\\temp\\") != NULL)
		return 1;
	if (wcscasestr(file, L"\\temp/") != NULL)
		return 1;
	if (wcscasestr(file, L"/temp\\") != NULL)
		return 1;
	if (wcscasestr(file, L"\\PIPE\\") != NULL)
		return 1;
	if (wcsstr(file, L"$") != NULL)
		return 1;
	return 0;
}

static void handle_file(const char* file, const char* file2, enum access_type at)
{
	char buf[ACCESS_EVENT_MAX_SIZE];
	size_t fsz = file ? strlen(file) : 0;
	size_t f2sz = file2 ? strlen(file2) : 0;
	struct access_event* e = (struct access_event*) buf;
	char* dest = (char*) (e + 1);
	int ret;

	if (ignore_file(file) || ignore_file(file2) || sock == INVALID_SOCKET)
		return;

	e->at = at;
	e->len = fsz;
	e->len2 = f2sz;

	memcpy(dest, file, fsz);
	dest += fsz;
	*(dest++) = '\0';

	memcpy(dest, file2, f2sz);
	dest += f2sz;
	*(dest++) = '\0';

	DEBUG_HOOK("%s: '%s' '%s'\n", access_type_name[at], file, file2);
	ret = send(sock, (char*) e, dest - (char*) e, 0);
	DEBUG_HOOK("send %d\n", ret);
}

static void handle_file_w(const wchar_t* file, const wchar_t* file2, enum access_type at)
{
	char buf[ACCESS_EVENT_MAX_SIZE];
	size_t fsz = file ? wcslen(file) : 0;
	size_t f2sz = file2 ? wcslen(file2) : 0;
	struct access_event* e = (struct access_event*) buf;
	char* dest = (char*) (e + 1);
	int ret;

	if (ignore_file_w(file) || ignore_file_w(file2) || sock == INVALID_SOCKET)
		return;

	e->at = at;

	e->len = WideCharToMultiByte(CP_ACP, 0, file, fsz, dest, fsz, NULL, NULL);
	dest += e->len;
	*(dest++) = L'\0';

	e->len2 = WideCharToMultiByte(CP_ACP, 0, file2, f2sz, dest, f2sz, NULL, NULL);
	dest += e->len2;
	*(dest++) = L'\0';

	DEBUG_HOOK("%s: '%S', '%S'\n", access_type_name[at], file, file2);
	ret = send(sock, (char*) e, dest - (char*) e, 0);
	DEBUG_HOOK("send %d\n", ret);
}

static int connect_udp(unsigned short udp_port)
{
	struct sockaddr_in sa;
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (sock == INVALID_SOCKET) {
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa.sin_port = htons(udp_port);

	DEBUG_HOOK("Connecting to udp localhost %d\n", udp_port);
	if (connect(sock, (struct sockaddr*) &sa, sizeof(struct sockaddr_in))) {
		goto err;
	}

	return 0;

err:
	closesocket(sock);
	return -1;
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
	WSADATA wsa_data;
	char filename[MAX_PATH];

	if (initialised)
		return 0;

	initialised = 1;

	if (!GetModuleFileNameA(NULL, filename, sizeof(filename))) {
		return 1;
	}

	DEBUG_HOOK("Inside tup_dllinject_init '%s' '%s' '%s' '%s' %d\n",
		filename,
		r->execdir,
		r->dll_name,
		r->func_name,
		(int) r->udp_port);

	DEBUG_HOOK("%d: %s\n", GetCurrentProcessId(), GetCommandLineA());

	if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &modnum)) {
		return 1;
	}

	modnum /= sizeof(HMODULE);

	for (i = 0; i < modnum; i++) {
		if (!GetModuleFileNameA(modules[i], filename, sizeof(filename))) {
			return 1;
		}

		foreach_module(modules[i], &have_kernel32_import, &have_advapi32_import);
	}

	tup_inject_setexecdir(r->execdir);

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
		return 1;

	if (connect_udp(r->udp_port))
		return 1;

	s_udp_port = r->udp_port;

	return 0;
}

static DWORD WINAPI remote_thread(void* param)
{
	HMODULE h;
	tup_init_t p;
	remote_thread_t* r = (remote_thread_t*) param;

	h = r->load_library(r->dll_name);
	if (!h) {
		return 1;
	}

	p = (tup_init_t) r->get_proc_address(h, r->func_name);
	if (!p) {
		return 1;
	}

	return p(r);
}

static void remote_thread_end(void)
{
}

int tup_inject_dll(
	LPPROCESS_INFORMATION lpProcessInformation,
	unsigned short udp_port)
{
	remote_thread_t remote;
	char* remote_data;
	size_t code_size;
	DWORD old_protect;
	HANDLE thread;
	HANDLE process;
	HMODULE kernel32;
	DWORD return_code;

	memset(&remote, 0, sizeof(remote));
	kernel32 = LoadLibraryA("kernel32.dll");
	remote.load_library = (LoadLibraryA_t) GetProcAddress(kernel32, "LoadLibraryA");
	remote.get_proc_address = (GetProcAddress_t) GetProcAddress(kernel32, "GetProcAddress");
	remote.udp_port = udp_port;
	strcat(remote.execdir, execdir);
	strcat(remote.dll_name, execdir);
	strcat(remote.dll_name, "\\");
	strcat(remote.dll_name, "tup-dllinject.dll");
	strcat(remote.func_name, "tup_inject_init");

	DEBUG_HOOK("Injecting dll '%s' '%s' %s' %d\n",
		remote.execdir,
		remote.dll_name,
		remote.func_name,
		udp_port);

	process = lpProcessInformation->hProcess;

	if (!WaitForInputIdle(process, INFINITE))
		return -1;

	/* Align code_size to a 16 byte boundary */
	code_size = (  (uintptr_t) &remote_thread_end
		     - (uintptr_t) &remote_thread + 0x0F)
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

	if (!WriteProcessMemory(process, remote_data, &remote_thread, code_size, NULL))
		return -1;

	if (!WriteProcessMemory(process, remote_data + code_size, &remote, sizeof(remote), NULL))
		return -1;

	if (!VirtualProtectEx(process, remote_data, code_size + sizeof(remote), PAGE_EXECUTE_READ, &old_protect))
		return -1;

	if (!FlushInstructionCache(process, remote_data, code_size + sizeof(remote)))
		return -1;

	thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE) remote_data, remote_data + code_size, 0, NULL);

	if (thread == INVALID_HANDLE_VALUE)
		return -1;

	if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0)
		return -1;

	if (!GetExitCodeThread(thread, &return_code))
		return -1;

	CloseHandle(thread);

	return return_code;
}
