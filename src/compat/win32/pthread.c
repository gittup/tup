#include "pthread.h"

typedef struct ThreadData ThreadData;

struct ThreadData
{
	pthread_start_t func;
	void*		arg;
	void*		ret;
};

static DWORD __stdcall begin_thread(void* arg)
{
	ThreadData* data = (ThreadData*) arg;
	data->ret = data->func(data->arg);
	return 0;
}

int pthread_create(
		pthread_t*		thread,
		const pthread_attr_t*	attr,
		pthread_start_t		func,
		void*			arg)
{
	ThreadData* d = (ThreadData*) malloc(sizeof(ThreadData));
	d->func = func;
	d->arg  = arg;
	d->ret  = NULL;

	(void) attr;
	thread->data = d;

	thread->h = CreateThread(NULL, 0, &begin_thread, d, 0, 0);

	return 0;
}

int pthread_join(
		pthread_t		thread,
		void**			retarg)
{
	HANDLE h = (HANDLE) thread.h;
	ThreadData* data = (ThreadData*) thread.data;
	if (retarg) {
		*retarg = data->ret;
	}
	WaitForSingleObject(h, INFINITE);
	CloseHandle(h);
	free(data);
	return 0;
}

int pthread_mutex_init(
		pthread_mutex_t*	mutex,
		const pthread_mutexattr_t* attr)
{
	(void) attr;
	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
}
