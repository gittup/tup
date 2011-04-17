#ifndef tup_fuse_fs_h
#define tup_fuse_fs_h

#define FUSE_USE_VERSION 26

#include <fuse.h>

struct file_info;

int tup_fuse_add_group(int pid, struct file_info *finfo);
int tup_fuse_rm_group(struct file_info *finfo);
extern struct fuse_operations tup_fs_oper;

#endif
