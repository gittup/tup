#ifndef tup_db_h
#define tup_db_h

#include "tupid.h"
#include "linux/list.h"
#include "linux/rbtree.h"
#include <stdio.h>
#include <time.h>

#define TUP_DIR ".tup"
#define TUP_DB_FILE ".tup/db"
#define TUP_VARDICT_FILE ".tup/vardict"
#define DOT_DT 1
#define VAR_DT 2

struct tup_entry;

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
int tup_db_check_flags(int flags);
int tup_db_check_dup_links(void);
void tup_db_enable_sql_debug(void);
int tup_db_debug_add_all_ghosts(void);

/* Node operations */
tupid_t tup_db_create_node(tupid_t dt, const char *name, int type);
tupid_t tup_db_create_node_part(tupid_t dt, const char *name, int len, int type);
tupid_t tup_db_node_insert(tupid_t dt, const char *name, int len, int type,
			   time_t mtime);
int tup_db_node_insert_tent(tupid_t dt, const char *name, int len, int type,
			    time_t mtime, struct tup_entry **entry);
int tup_db_fill_tup_entry(tupid_t tupid, struct tup_entry *tent);
int tup_db_select_tent(tupid_t dt, const char *name, struct tup_entry **entry);
int tup_db_select_tent_part(tupid_t dt, const char *name, int len,
			    struct tup_entry **entry);
int tup_db_select_node_by_flags(int (*callback)(void *, struct tup_entry *,
						int style),
				void *arg, int flags);
int tup_db_select_node_dir(int (*callback)(void *, struct tup_entry *, int style),
			   void *arg, tupid_t dt);
int tup_db_select_node_dir_glob(int (*callback)(void *, struct tup_entry *),
				void *arg, tupid_t dt, const char *glob,
				int len);
int tup_db_delete_node(tupid_t tupid);
int tup_db_delete_dir(tupid_t dt);
int tup_db_modify_dir(tupid_t dt);
int tup_db_open_tupid(tupid_t dt);
int tup_db_is_root_node(tupid_t tupid);
int tup_db_change_node(tupid_t tupid, const char *name, tupid_t new_dt);
int tup_db_set_name(tupid_t tupid, const char *new_name);
int tup_db_set_type(struct tup_entry *tent, int type);
int tup_db_set_sym(struct tup_entry *tent, tupid_t sym);
int tup_db_set_mtime(struct tup_entry *tent, time_t mtime);
int tup_db_print(FILE *stream, tupid_t tupid);
int tup_db_alloc_generated_nodelist(char **s, int *len, tupid_t dt,
				    struct rb_root *tree);
int tup_db_delete_gitignore(tupid_t dt, struct rb_root *tree, int *count);

/* Flag operations */
int tup_db_get_node_flags(tupid_t tupid);
int tup_db_add_dir_create_list(tupid_t tupid);
int tup_db_add_create_list(tupid_t tupid);
int tup_db_add_modify_list(tupid_t tupid);
int tup_db_in_create_list(tupid_t tupid);
int tup_db_in_modify_list(tupid_t tupid);
int tup_db_unflag_create(tupid_t tupid);
int tup_db_unflag_modify(tupid_t tupid);

/* Link operations */
int tup_db_create_link(tupid_t a, tupid_t b, int style);
int tup_db_create_unique_link(tupid_t a, tupid_t b, struct rb_root *tree);
int tup_db_link_exists(tupid_t a, tupid_t b);
int tup_db_link_style(tupid_t a, tupid_t b, int *style);
int tup_db_get_incoming_link(tupid_t tupid, tupid_t *incoming);
int tup_db_delete_links(tupid_t tupid);

/* Combo operations */
int tup_db_modify_cmds_by_output(tupid_t output, int *modified);
int tup_db_modify_cmds_by_input(tupid_t input);
int tup_db_set_dependent_dir_flags(tupid_t tupid);
int tup_db_select_node_by_link(int (*callback)(void *, struct tup_entry *,
					       int style),
			       void *arg, tupid_t tupid);
int tup_db_delete_dependent_dir_links(tupid_t tupid);

/* Config operations */
int tup_db_show_config(void);
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
int tup_db_var_foreach(int (*callback)(void *, const char *var, const char *value, int type), void *arg);
int tup_db_read_vars(tupid_t dt, const char *file);

/* Tree operations */
int tup_db_cmds_to_tree(tupid_t dt, struct rb_root *tree, int *count);
int tup_db_cmd_outputs_to_tree(tupid_t dt, struct rb_root *tree, int *count);

/* tmp table management */
int tup_db_request_tmp_list(void);
int tup_db_release_tmp_list(void);
int tup_db_clear_tmp_list(void);

/* scanner operations */
int tup_db_scan_begin(struct rb_root *tree);
int tup_db_scan_end(struct rb_root *tree);

/* updater tmp operations */
int tup_db_add_write_list(tupid_t tupid);
int tup_db_check_write_list(tupid_t cmdid);
int tup_db_check_actual_inputs(tupid_t cmdid, struct list_head *readlist);
int tup_db_write_outputs(tupid_t cmdid);
int tup_db_write_inputs(tupid_t cmdid, struct rb_root *input_tree);

#endif
