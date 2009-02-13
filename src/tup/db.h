#ifndef tup_db_h
#define tup_db_h

#include "tupid.h"

#define TUP_DIR ".tup"
#define TUP_DB_FILE ".tup/db"
#define DOT_DT 1
#define VAR_DT 2

struct db_node {
	tupid_t tupid;
	tupid_t dt;
	const char *name;
	int type;
};

enum TUP_NODE_TYPE {
	TUP_NODE_FILE,
	TUP_NODE_CMD,
	TUP_NODE_DIR,
	TUP_NODE_VAR,
	TUP_NODE_ROOT,
};

enum TUP_FLAGS_TYPE {
	TUP_FLAGS_NONE=0,
	TUP_FLAGS_MODIFY=1,
	TUP_FLAGS_CREATE=2,
	TUP_FLAGS_DELETE=4,
};

/* General operations */
int tup_db_open(void);
int tup_db_close(void);
int tup_db_create(int db_sync);
int tup_db_begin(void);
int tup_db_commit(void);
int tup_db_rollback(void);

int tup_db_select(int (*callback)(void *, int, char **, char **), void *arg,
		  const char *sql, ...);

/* Node operations */
tupid_t tup_db_create_node(tupid_t dt, const char *name, int type);
tupid_t tup_db_create_node_part(tupid_t dt, const char *name, int len, int type);
tupid_t tup_db_create_dup_node(tupid_t dt, const char *name, int type);
tupid_t tup_db_select_node(tupid_t dt, const char *name);
tupid_t tup_db_select_dbn(tupid_t dt, const char *name, struct db_node *dbn);
tupid_t tup_db_select_node_part(tupid_t dt, const char *name, int len);
int tup_db_select_node_by_flags(int (*callback)(void *, struct db_node *),
				void *arg, int flags);
int tup_db_select_node_dir(int (*callback)(void *, struct db_node *), void *arg,
			   tupid_t dt);
int tup_db_select_node_dir_glob(int (*callback)(void *, struct db_node *),
				void *arg, tupid_t dt, const char *glob);
int tup_db_set_flags_by_name(tupid_t dt, const char *name, int flags);
int tup_db_set_flags_by_id(tupid_t tupid, int flags);
int tup_db_delete_node(tupid_t tupid);
int tup_db_delete_dir(tupid_t dt);
int tup_db_open_tupid(tupid_t dt);
int tup_db_get_path(tupid_t tupid, char *path, int len);
tupid_t tup_db_parent(tupid_t tupid);

/* Flag operations */
int tup_db_get_node_flags(tupid_t tupid);
int tup_db_add_create_list(tupid_t tupid);
int tup_db_add_modify_list(tupid_t tupid);
int tup_db_add_delete_list(tupid_t tupid);
int tup_db_in_create_list(tupid_t tupid);
int tup_db_in_modify_list(tupid_t tupid);
int tup_db_in_delete_list(tupid_t tupid);
int tup_db_unflag_create(tupid_t tupid);
int tup_db_unflag_modify(tupid_t tupid);
int tup_db_unflag_delete(tupid_t tupid);

/* Link operations */
int tup_db_create_link(tupid_t a, tupid_t b);
int tup_db_create_unique_link(tupid_t a, tupid_t b);
int tup_db_link_exists(tupid_t a, tupid_t b);
int tup_db_is_root_node(tupid_t tupid);
int tup_db_delete_links(tupid_t tupid);

/* Combo operations */
int tup_db_or_dircmd_flags(tupid_t parent, int flags, int type);
int tup_db_set_cmd_output_flags(tupid_t parent, int flags);
int tup_db_set_cmd_flags_by_output(tupid_t output, int flags);
int tup_db_set_dependent_dir_flags(tupid_t tupid);
int tup_db_select_node_by_link(int (*callback)(void *, struct db_node *),
			       void *arg, tupid_t tupid);
int tup_db_delete_dependent_dir_links(tupid_t tupid);

/* Config operations */
int tup_db_config_set_int(const char *lval, int x);
int tup_db_config_get_int(const char *lval);
int tup_db_config_set_string(const char *lval, const char *rval);
int tup_db_config_get_string(char **res, const char *lval, const char *def);

/* Var operations */
int tup_db_set_var(tupid_t tupid, const char *value);
tupid_t tup_db_get_var(const char *var, int varlen, char **dest);
int tup_db_get_var_id(tupid_t tupid, char **dest);
int tup_db_get_varlen(const char *var, int varlen);
tupid_t tup_db_write_var(const char *var, int varlen, int fd);
int tup_db_var_foreach(int (*callback)(void *, const char *var, const char *value), void *arg);

#endif
