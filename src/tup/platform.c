#include "platform.h"

#ifdef __linux__
const char *tup_platform = "linux";
#else
#error Unsupported platform. Please add support in tup/platform.c
#endif
