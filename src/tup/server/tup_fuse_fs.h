#ifndef tup_fuse_fs_h
#define tup_fuse_fs_h

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <pthread.h>

extern pthread_key_t fuse_key;
extern struct fuse_operations tup_fs_oper;

#endif
