/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef tup_db_h
#define tup_db_h

#include "db_types.h"
#include "tupid.h"
#include "tupid_tree.h"
#include "bsd/queue.h"
#include <stdio.h>
#include <time.h>

struct tup_entry;
struct tup_entry_head;
struct tup_env;

/* General operations */
int tup_db_open(void);
int tup_db_close(void);
int tup_db_create(int db_sync);
int tup_db_begin(void);
int tup_db_commit(void);
int tup_db_rollback(void);
int tup_db_check_flags(int flags);
void tup_db_enable_sql_debug(void);
int tup_db_debug_add_all_ghosts(void);
const char *tup_db_type(enum TUP_NODE_TYPE type);

/* Node operations */
struct tup_entry *tup_db_create_node(tupid_t dt, const char *name, int type);
struct tup_entry *tup_db_create_node_part(tupid_t dt, const char *name, int len,
					  int type);
struct tup_entry *tup_db_node_insert(tupid_t dt, const char *name,
				     int len, int type, time_t mtime);
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
				int len, struct tupid_entries *delete_root);
int tup_db_delete_node(tupid_t tupid);
int tup_db_delete_dir(tupid_t dt);
int tup_db_modify_dir(tupid_t dt);
int tup_db_get_generated_tup_entries(tupid_t dt, struct tup_entry_head *head);
int tup_db_open_tupid(tupid_t dt);
int tup_db_change_node(tupid_t tupid, const char *name, tupid_t new_dt);
int tup_db_set_name(tupid_t tupid, const char *new_name);
int tup_db_set_type(struct tup_entry *tent, int type);
int tup_db_set_mtime(struct tup_entry *tent, time_t mtime);
int tup_db_print(FILE *stream, tupid_t tupid);
int tup_db_alloc_generated_nodelist(char **s, int *len, tupid_t dt,
				    struct tupid_entries *root);
int tup_db_rebuild_all(void);
int tup_db_delete_slash(void);
tupid_t slash_dt(void);

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
int tup_db_create_unique_link(tupid_t a, tupid_t b, struct tupid_entries *delroot,
			      struct tupid_entries *root);
int tup_db_link_exists(tupid_t a, tupid_t b);
int tup_db_link_style(tupid_t a, tupid_t b, int *style);
int tup_db_get_incoming_link(tupid_t tupid, tupid_t *incoming);
int tup_db_delete_links(tupid_t tupid);
int tup_db_write_outputs(tupid_t cmdid, struct tupid_entries *root);
int tup_db_write_inputs(tupid_t cmdid, struct tupid_entries *input_root,
			struct tupid_entries *env_root,
			struct tupid_entries *delete_root);
int tup_db_write_dir_inputs(tupid_t dt, struct tupid_entries *root);
int tup_db_get_links(tupid_t cmdid, struct tupid_entries *sticky_root,
		     struct tupid_entries *normal_root);

/* Combo operations */
int tup_db_modify_cmds_by_output(tupid_t output, int *modified);
int tup_db_modify_cmds_by_input(tupid_t input);
int tup_db_set_dependent_dir_flags(tupid_t tupid);
int tup_db_select_node_by_link(int (*callback)(void *, struct tup_entry *,
					       int style),
			       void *arg, tupid_t tupid);

/* Config operations */
int tup_db_show_config(void);
int tup_db_config_set_int(const char *lval, int x);
int tup_db_config_get_int(const char *lval, int def, int *result);
int tup_db_config_set_string(const char *lval, const char *rval);

/* Var operations */
int tup_db_set_var(tupid_t tupid, const char *value);
struct tup_entry *tup_db_get_var(const char *var, int varlen, char **dest);
int tup_db_get_var_id_alloc(tupid_t tupid, char **dest);
int tup_db_get_varlen(const char *var, int varlen);
int tup_db_var_foreach(tupid_t dt, int (*callback)(void *, tupid_t tupid, const char *var, const char *value, int type), void *arg);
int tup_db_read_vars(tupid_t dt, const char *file);

/* Environment operations */
int tup_db_check_env(int environ_check);
int tup_db_findenv(const char *var, struct tup_entry **tent);
int tup_db_get_environ(struct tupid_entries *root,
		       struct tupid_entries *normal_root, struct tup_env *te);
tupid_t env_dt(void);

/* Tree operations */
int tup_db_dirtype_to_tree(tupid_t dt, struct tupid_entries *root, int *count, int type);

/* scanner operations */
int tup_db_scan_begin(struct tupid_entries *root);
int tup_db_scan_end(struct tupid_entries *root);

/* updater operations */
int tup_db_check_actual_outputs(FILE *f, tupid_t cmdid,
				struct tup_entry_head *writehead);
int tup_db_check_actual_inputs(FILE *f, tupid_t cmdid,
			       struct tup_entry_head *readhead,
			       struct tupid_entries *sticky_root,
			       struct tupid_entries *normal_root);

#endif
