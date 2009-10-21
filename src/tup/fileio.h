#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"
#include "linux/list.h"
#include <time.h>

#define TUP_CONFIG "tup.config"

struct db_node;
struct tup_entry;

struct path_element {
	struct list_head list;
	const char *path; /* Not nul-terminated */
	int len;
};

#define PG_HIDDEN 1
#define PG_OUTSIDE_TUP 2
#define PG_ROOT 4
struct pel_group {
	struct list_head path_list;
	int pg_flags;
};

tupid_t create_name_file(tupid_t dt, const char *file, time_t mtime);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t update_symlink_fileat(tupid_t dt, int dfd, const char *file,
			      time_t mtime, int force);
tupid_t tup_file_mod(tupid_t dt, const char *file);
tupid_t tup_file_mod_mtime(tupid_t dt, const char *file, time_t mtime,
			   int force);
int tup_file_del(tupid_t dt, const char *file, int len);
int tup_file_missing(tupid_t tupid, int type);
int tup_del_id_force(tupid_t tupid, int type);
tupid_t get_dbn_dt(tupid_t dt, const char *path, struct db_node *dbn);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir,
			  struct path_element **last, struct list_head *symlist,
			  int sotgv);
tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     struct path_element **last,
			     struct list_head *symlist, int sotgv);
int add_node_to_list(tupid_t dt, struct pel_group *pg, struct list_head *list,
		     int sotgv);
int gimme_node_or_make_ghost(tupid_t dt, const char *name,
			     struct list_head *symlist,
			     struct tup_entry **entry);

int delete_file(tupid_t dt, const char *name);
int delete_name_file(tupid_t tupid);

int get_path_elements(const char *dir, struct pel_group *pg);
int pg_eq(const struct pel_group *pga, const struct pel_group *pgb);
void del_pel(struct path_element *pel);
void del_pel_list(struct list_head *list);

#endif
