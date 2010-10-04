#ifndef tup_dllinject_h
#define tup_dllinject_h

#include <windows.h>

#ifdef BUILDING_DLLINJECT
# define DLLINJECT_API __declspec(dllexport)
#else
# define DLLINJECT_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLLINJECT_API BOOL WINAPI DllMain(HANDLE HDllHandle, DWORD Reason, LPVOID Reserved);

typedef struct remote_thread_t remote_thread_t;
DLLINJECT_API DWORD tup_inject_init(remote_thread_t* r);

DLLINJECT_API void tup_inject_setexecdir(
	const char* dir);

DLLINJECT_API int tup_inject_dll(
	LPPROCESS_INFORMATION process,
	unsigned short udp_port);

#ifdef __cplusplus
}
#endif

#endif
