#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/** Lock file used to synchronize the monitor with the wrapper scripts */
#define TUP_LOCK ".tup/lock"

#endif
