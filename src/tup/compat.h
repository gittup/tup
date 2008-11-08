#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/** Lock file used to synchronize the monitor with the wrapper scripts */
#define TUP_OBJECT_LOCK ".tup/object"

/** Lock file used to ensure only one updater runs at a time */
#define TUP_UPDATE_LOCK ".tup/updater"

#endif
