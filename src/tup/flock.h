#ifndef tup_flock_h
#define tup_flock_h

/* Wrappers for fcntl */
int tup_flock(int fd);
int tup_unflock(int fd);

#endif
