#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"
#include "bsd/queue.h"
#include <time.h>

#define TUP_CONFIG "tup.config"

struct tup_entry;
struct tup_entry_head;
struct path_element;
struct pel_group;

int create_name_file(tupid_t dt, const char *file, time_t mtime,
		     struct tup_entry **entry);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t tup_file_mod(tupid_t dt, const char *file);
tupid_t tup_file_mod_mtime(tupid_t dt, const char *file, time_t mtime,
			   int force);
int tup_file_del(tupid_t dt, const char *file, int len);
int tup_file_missing(struct tup_entry *tent);
int tup_del_id_force(tupid_t tupid, int type);
void tup_register_rmdir_callback(void (*callback)(tupid_t tupid));
struct tup_entry *get_tent_dt(tupid_t dt, const char *path);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir,
			  struct path_element **last, int sotgv);
tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     struct path_element **last, int sotgv);
int gimme_tent(const char *name, struct tup_entry **entry);
int gimme_tent_or_make_ghost(tupid_t dt, const char *name,
			     struct tup_entry **entry);

int delete_file(tupid_t dt, const char *name);
int delete_name_file(tupid_t tupid);

#endif
