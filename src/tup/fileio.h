#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"

struct db_node;
struct list_head;

tupid_t create_name_file(tupid_t dt, const char *file);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t update_symlink_file(tupid_t dt, const char *file);
tupid_t create_var_file(const char *var, const char *value);
tupid_t tup_file_mod(tupid_t dt, const char *file, int flags);
int tup_file_del(tupid_t tupid, tupid_t dt, int type);
tupid_t get_dbn_dt(tupid_t dt, const char *path, struct db_node *dbn,
		   struct list_head *symlist);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir, const char **last,
			  struct list_head *symlist);

int delete_name_file(tupid_t tupid);

#endif
