#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"
#include "list.h"

struct db_node;

struct path_element {
	struct list_head list;
	const char *path; /* Not nul-terminated */
	int len;
};

#define PG_HIDDEN 1
#define PG_OUTSIDE_TUP 2
struct pel_group {
	struct list_head path_list;
	int pg_flags;
};

tupid_t create_name_file(tupid_t dt, const char *file);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t update_symlink_fileat(tupid_t dt, int dfd, const char *file);
tupid_t create_var_file(const char *var, const char *value);
tupid_t tup_file_exists(tupid_t dt, const char *file);
tupid_t tup_file_mod(tupid_t dt, const char *file);
int tup_file_del(tupid_t dt, const char *file);
int tup_del_id(tupid_t tupid, tupid_t dt, tupid_t sym, int type);
tupid_t get_dbn_dt(tupid_t dt, const char *path, struct db_node *dbn,
		   struct list_head *symlist);
tupid_t get_dbn_dt_pg(tupid_t dt, struct pel_group *pg, struct db_node *dbn,
		      struct list_head *symlist);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir, const char **last,
			  struct list_head *symlist, int sotgv);
tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     const char **last, struct list_head *symlist,
			     int sotgv);

int delete_file(tupid_t dt, const char *name);
int delete_name_file(tupid_t tupid, tupid_t dt, tupid_t sym);

int get_path_elements(const char *dir, struct pel_group *pg);
int pg_eq(const struct pel_group *pga, const struct pel_group *pgb);
void del_pel(struct path_element *pel);
void del_pel_list(struct list_head *list);
int sym_follow(struct db_node *dbn, struct list_head *symlist);

#endif
