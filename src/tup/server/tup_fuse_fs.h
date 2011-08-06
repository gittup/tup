#ifndef tup_fuse_fs_h
#define tup_fuse_fs_h

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include "tup/tupid.h"

#define TUP_TMP ".tup/tmp"

struct file_info;
struct rb_root;

int tup_fuse_add_group(int id, struct file_info *finfo);
int tup_fuse_rm_group(struct file_info *finfo);
void tup_fuse_set_parser_mode(int mode, struct rb_root *delete_tree);
tupid_t tup_fuse_server_get_curid(void);
extern struct fuse_operations tup_fs_oper;

#endif
