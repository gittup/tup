/* Unsupported in win32 */
#define MAP_FAILED ((void*)-1)
#define mmap(a, b, c, d, e, f) MAP_FAILED
