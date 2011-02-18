#include "platform.h"

/* NOTE: Please keep the list in tup.1 in sync */
#ifdef __linux__
const char *tup_platform = "linux";
#elif __sun__
const char *tup_platform = "solaris";
#elif __APPLE__
const char *tup_platform = "macosx";
#elif _WIN32
const char *tup_platform = "win32";
#else
#error Unsupported platform. Please add support in tup/platform.c
#endif


/* NOTE: Please keep the list in tup.1 in sync */

#ifdef __x86_64__
const char *tup_arch = "x86_64";
#elif __i386__
const char *tup_arch = "i386";
#elif __powerpc__
const char *tup_arch = "powerpc";
#elif __powerpc64__
const char *tup_arch = "powerpc64";
#elif __ia64__
const char *tup_arch = "ia64";
#elif __alpha__
const char *tup_arch = "alpha";
#elif __sparc__
const char *tup_arch = "sparc";
#elif __arm__
const char *tup_arch = "arm";
#else
#error Unsupported cpu architecture. Please add support in tup/platform.c
#endif
