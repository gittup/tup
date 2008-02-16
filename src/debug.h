#ifndef debug_h
#define debug_h

#include <unistd.h>

/** Environment variable to set if debugging is enabled. */
#define TUP_DEBUG "tup_debug"
#define DEBUGP(format, args...) if(debug_enabled()) fprintf(stderr, "[34m[%s:%i %s:%i][0m " format, debug_string(), getpid(), __FILE__, __LINE__, ##args)

int debug_enabled(void);
const char *debug_string(void);
void debug_enable(const char *label);
void debug_disable(void);

#endif
