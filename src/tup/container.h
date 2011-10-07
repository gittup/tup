#include <stdint.h>
#ifndef offsetof
#define offsetof __builtin_offsetof
#endif
#define container_of(p, stype, field) ((stype *)(void *)(((uint8_t *)(p)) - offsetof(stype, field)))
