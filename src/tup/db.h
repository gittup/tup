#ifndef tup_db_h
#define tup_db_h

#include "tupid.h"
#include "list.h"

#define TUP_DIR ".tup"
#define TUP_DB_FILE ".tup/db"
#define TUP_VARDICT_FILE ".tup/vardict"
#define DOT_DT 1
#define VAR_DT 2

struct db_node {
	tupid_t tupid;
	tupid_t dt;
	const char *name;
	int type;
	tupid_t sym;
};

struct id_entry {
	struct list_head list;
	tupid_t tupid;
};

/* This is kinda like a combination of db_node and id_entry, except we don't
 * have 'name'. This is used in symlists, as well as to get a list of nodes to
 * delete back to the monitor so it can handle all the deletions that happened
 * while we were out fishing (or whatever it is programs do when they're not
 * working).
 */
struct half_entry {
	struct list_head list;
	tupid_t tupid;
	tupid_t dt;
	tupid_t sym;
	int type;
};

enum TUP_NODE_TYPE {
	TUP_NODE_FILE,
	TUP_NODE_CMD,
	TUP_NODE_DIR,
	TUP_NODE_VAR,
	TUP_NODE_GENERATED,
	TUP_NODE_GHOST,
	TUP_NODE_ROOT,
};

enum TUP_FLAGS_TYPE {
	TUP_FLAGS_NONE=0,
	TUP_FLAGS_MODIFY=1,
	TUP_FLAGS_CREATE=2,
	TUP_FLAGS_DELETE=4,
};

enum TUP_LINK_TYPE {
	TUP_LINK_NORMAL=1,
	TUP_LINK_STICKY=2,
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
tupid_t tup_db_node_insert(tupid_t dt, const char *name, int len, int type);
int tup_db_select_dbn_by_id(tupid_t tupid, struct db_node *dbn);
int tup_db_select_dbn(tupid_t dt, const char *name, struct db_node *dbn);
int tup_db_select_dbn_part(tupid_t dt, const char *name, int len,
			   struct db_node *dbn);
int tup_db_select_node_by_flags(int (*callback)(void *, struct db_node *,
						int style),
				void *arg, int flags);
int tup_db_select_node_dir(int (*callback)(void *, struct db_node *, int style),
			   void *arg, tupid_t dt);
int tup_db_select_node_dir_glob(int (*callback)(void *, struct db_node *),
				void *arg, tupid_t dt, const char *glob);
int tup_db_set_flags_by_name(tupid_t dt, const char *name, int flags);
int tup_db_set_flags_by_id(tupid_t tupid, int flags);
int tup_db_delete_node(tupid_t tupid, tupid_t dt, tupid_t sym);
int tup_db_delete_dir(tupid_t dt);
int tup_db_modify_dir(tupid_t dt);
int tup_db_open_tupid(tupid_t dt);
int tup_db_get_path(tupid_t tupid, char *path, int len);
tupid_t tup_db_parent(tupid_t tupid);
int tup_db_is_root_node(tupid_t tupid);
int tup_db_change_node(tupid_t tupid, const char *name, tupid_t new_dt);
int tup_db_set_type(tupid_t tupid, int type);
int tup_db_set_sym(tupid_t tupid, tupid_t sym);

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
int tup_db_create_link(tupid_t a, tupid_t b, int style);
int tup_db_create_unique_link(tupid_t a, tupid_t b);
int tup_db_delete_empty_links(tupid_t tupid);
int tup_db_yell_links(tupid_t tupid, const char *errmsg);
int tup_db_link_exists(tupid_t a, tupid_t b);
int tup_db_link_style(tupid_t a, tupid_t b);
int tup_db_get_incoming_link(tupid_t tupid, tupid_t *incoming);
int tup_db_delete_links(tupid_t tupid);
int tup_db_unsticky_links(tupid_t tupid);

/* Combo operations */
int tup_db_flag_delete_in_dir(tupid_t dt, int type);
int tup_db_flag_delete_cmd_outputs(tupid_t dt);
int tup_db_remove_output_links(tupid_t dt);
int tup_db_modify_cmds_by_output(tupid_t output);
int tup_db_modify_cmds_by_input(tupid_t input);
int tup_db_set_dependent_dir_flags(tupid_t tupid);
int tup_db_modify_deleted_deps(void);
int tup_db_select_node_by_link(int (*callback)(void *, struct db_node *,
					       int style),
			       void *arg, tupid_t tupid);
int tup_db_delete_dependent_dir_links(tupid_t tupid);

/* Config operations */
int tup_db_config_set_int(const char *lval, int x);
int tup_db_config_get_int(const char *lval);
int tup_db_config_set_int64(const char *lval, sqlite3_int64 x);
sqlite3_int64 tup_db_config_get_int64(const char *lval);
int tup_db_config_set_string(const char *lval, const char *rval);
int tup_db_config_get_string(char **res, const char *lval, const char *def);

/* Var operations */
int tup_db_set_var(tupid_t tupid, const char *value);
tupid_t tup_db_get_var(const char *var, int varlen, char **dest);
int tup_db_get_var_id_alloc(tupid_t tupid, char **dest);
int tup_db_get_varlen(const char *var, int varlen);
tupid_t tup_db_write_var(const char *var, int varlen, int fd);
int tup_db_var_foreach(int (*callback)(void *, const char *var, const char *value), void *arg);
int tup_db_write_vars(void);
int tup_db_var_pre(void);
int tup_db_var_post(void);
int tup_db_remove_var_list(tupid_t tupid);
int tup_db_create_var_list(void);
int tup_db_delete_var_list(void);

/* tmpdb operations */
int tup_db_scan_begin(void);
int tup_db_scan_end(void);
int tup_db_attach_tmpdb(void);
int tup_db_detach_tmpdb(void);
int tup_db_files_to_tmpdb(void);
int tup_db_unflag_tmpdb(tupid_t tupid);
int tup_db_get_all_in_tmpdb(struct list_head *list);

/* updater temp operations */
int tup_db_create_tmp_tables(void);
int tup_db_drop_tmp_tables(void);
int tup_db_clear_tmp_tables(void);
int tup_db_add_write_list(tupid_t tupid);
int tup_db_check_write_list(tupid_t cmdid);
int tup_db_add_read_list(tupid_t tupid);
int tup_db_check_read_list(tupid_t cmdid);

#endif
