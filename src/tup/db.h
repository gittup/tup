#ifndef tup_db_h
#define tup_db_h

#include "tupid.h"

#define TUP_DB_FILE ".tup/db"

enum TUP_NODE_TYPE {
	TUP_NODE_FILE,
	TUP_NODE_CMD,
	TUP_NODE_DIR,
	TUP_NODE_ROOT,
};

enum TUP_FLAGS_TYPE {
	TUP_FLAGS_NONE=0,
	TUP_FLAGS_MODIFY=1,
	TUP_FLAGS_CREATE=2,
	TUP_FLAGS_DELETE=4,
};

/* General operations */
int tup_open_db(void);
int tup_create_db(void);
int tup_db_begin(void);
int tup_db_commit(void);

int tup_db_exec(const char *sql, ...);
int tup_db_select(int (*callback)(void *, int, char **, char **), void *arg,
		  const char *sql, ...);

/* Node operations */
tupid_t tup_db_create_node(const char *name, int type, int flags);
tupid_t tup_db_select_node(const char *name);
int tup_db_set_flags_by_name(const char *name, int flags);
int tup_db_set_flags_by_id(tupid_t tupid, int flags);
int tup_db_delete_node(tupid_t tupid);

/* Link operations */
int tup_db_create_link(tupid_t a, tupid_t b);
int tup_db_link_exists(tupid_t a, tupid_t b);
int tup_db_delete_links(tupid_t tupid);

int tup_db_create_cmdlink(tupid_t a, tupid_t b);
int tup_db_cmdlink_exists(tupid_t a, tupid_t b);
int tup_db_delete_cmdlinks(tupid_t tupid);

/* Config operations */
int tup_db_config_set_int(const char *lval, int x);
int tup_db_config_get_int(const char *lval);

#endif
