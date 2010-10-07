#include "dir_mutex.h"
#include "tup/compat.h"

pthread_mutex_t dir_mutex;

int compat_init(void)
{
	if(pthread_mutex_init(&dir_mutex, NULL) < 0)
		return -1;
	return 0;
}
