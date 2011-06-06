#ifndef tup_compat_dirmutex_h
#define tup_compat_dirmutex_h

#include <pthread.h>

extern pthread_mutex_t dir_mutex;

int dir_mutex_lock(int dfd);
void dir_mutex_unlock(int handle);

#endif
