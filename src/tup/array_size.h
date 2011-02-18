#ifndef tup_array_size_h
#define tup_array_size_h

#define ARRAY_SIZE(n) ((signed)(sizeof(n) / sizeof(n[0])))

#endif
