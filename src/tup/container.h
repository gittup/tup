#define offsetof __builtin_offsetof
#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field)))
