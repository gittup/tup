#ifndef tup_win32_wow64_h
#define tup_win32_wow64_h
#define WOW64_CONTEXT_i386 0x00010000
#define WOW64_CONTEXT_CONTROL (WOW64_CONTEXT_i386 | __MSABI_LONG(0x00000001))

BOOL WINAPI Wow64GetThreadContext(HANDLE hThread, PWOW64_CONTEXT lpContext);
BOOL WINAPI Wow64SetThreadContext(HANDLE hThread, const WOW64_CONTEXT *lpContext);

#endif
