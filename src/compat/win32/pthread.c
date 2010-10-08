#include "pthread.h"
#include <stdio.h>

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

/* Condition variable code is from:
 * http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
 *
 * Modified to remove broadcasting.
 */
int pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t *attr)
{
	if(attr) {}
	cv->waiters_count = 0;

	pthread_mutex_init(&cv->waiters_count_lock, NULL);

	cv->event = CreateEvent(NULL,  /* no security */
				FALSE, /* auto-reset event */
				FALSE, /* non-signaled initially */
				NULL); /* unnamed */
	if(cv->event)
		return 0;
	return -1;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	CloseHandle(cond->event);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *external_mutex)
{
	int result;

	EnterCriticalSection(&cv->waiters_count_lock);
	cv->waiters_count++;
	LeaveCriticalSection(&cv->waiters_count_lock);

	/* It's ok to release the <external_mutex> here since Win32
	 * manual-reset events maintain state when used with
	 * <SetEvent>.  This avoids the "lost wakeup" bug...
	 */
	LeaveCriticalSection(external_mutex);

	/* Wait for the event to become signaled due to pthread_cond_signal */
	result = WaitForSingleObject(cv->event, INFINITE);
	if(result < 0) {
		fprintf(stderr, "tup error: WaitForSingleObject failed.\n");
		return -1;
	}

	EnterCriticalSection(&cv->waiters_count_lock);
	cv->waiters_count--;
	LeaveCriticalSection(&cv->waiters_count_lock);

	/* Reacquire the <external_mutex>. */
	EnterCriticalSection(external_mutex);
	return 0;
}

int pthread_cond_signal (pthread_cond_t *cv)
{
	int have_waiters;

	EnterCriticalSection(&cv->waiters_count_lock);
	have_waiters = cv->waiters_count > 0;
	LeaveCriticalSection(&cv->waiters_count_lock);

	if(have_waiters)
		SetEvent(cv->event);
	return 0;
}
