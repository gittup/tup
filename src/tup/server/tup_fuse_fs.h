#ifndef tup_fuse_fs_h
#define tup_fuse_fs_h

#define FUSE_USE_VERSION 26

#include <fuse.h>

#define TUP_TMP ".tup/tmp"

struct file_info;

int tup_fuse_add_group(int id, struct file_info *finfo);
int tup_fuse_rm_group(struct file_info *finfo);
void tup_fuse_enable_debug(void);
int tup_fuse_debug_enabled(void);
extern struct fuse_operations tup_fs_oper;

#endif
