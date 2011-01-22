#ifndef tup_pel_group_h
#define tup_pel_group_h

#include "tupid.h"
#include "linux/list.h"

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
	int num_elements;
};

void init_pel_group(struct pel_group *pg);
int split_path_elements(const char *dir, struct pel_group *pg);
int get_path_tupid(struct pel_group *pg, tupid_t *tupid);
int get_path_elements(const char *dir, struct pel_group *pg);
int append_path_elements(struct pel_group *pg, tupid_t dt);
int pg_eq(const struct pel_group *pga, const struct pel_group *pgb);
void del_pel(struct path_element *pel, struct pel_group *pg);
void del_pel_group(struct pel_group *pg);
void print_pel_group(struct pel_group *pg);

#endif
