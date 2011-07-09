#include "pel_group.h"
#include "db.h"
#include "config.h"
#include "entry.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_pel(const char *path, int len, struct pel_group *pg)
{
	struct path_element *pel;

	pel = malloc(sizeof *pel);
	if(!pel) {
		perror("malloc");
		return -1;
	}
	pel->path = path;
	pel->len = len;
	list_add_tail(&pel->list, &pg->path_list);
	return 0;
}

void init_pel_group(struct pel_group *pg)
{
	pg->pg_flags = 0;
	pg->num_elements = 0;
	INIT_LIST_HEAD(&pg->path_list);
}

int split_path_elements(const char *dir, struct pel_group *pg)
{
	struct path_element *pel;
	const char *p = dir;

	if(is_path_sep(dir)) {
		del_pel_group(pg);
		pg->pg_flags = PG_ROOT;
	}

	while(1) {
		const char *path;
		int len;
		while(*p && is_path_sep(p)) {
			p++;
		}
		if(!*p)
			break;
		path = p;
		while(*p && !is_path_sep(p)) {
			p++;
		}
		len = p - path;
		if(path[0] == '.') {
			if(len == 1) {
				/* Skip extraneous "." paths */
				continue;
			}
			if(path[1] == '.' && len == 2) {
				/* If it's a ".." path, then delete the
				 * previous entry, if any. Otherwise we just
				 * include it if it's at the beginning of the
				 * path.
				 */
				if(pg->num_elements) {
					pel = list_entry(pg->path_list.prev, struct path_element, list);
					del_pel(pel, pg);
					continue;
				}
				/* Don't set num_elements, since a ".." path
				 * can't be deleted by a subsequent ".."
				 */
				goto skip_num_elements;
			} else if(len == sizeof(".gitignore") - 1 &&
				  strncmp(path, ".gitignore", len) == 0) {
				/* .gitignore files are not considered hidden */
			} else {
				/* Hidden paths have special treatment in tup */
				pg->pg_flags |= PG_HIDDEN;
			}
		}

		pg->num_elements++;
skip_num_elements:

		if(add_pel(path, len, pg) < 0)
			return -1;
	}
	return 0;
}

int get_path_tupid(struct pel_group *pg, tupid_t *tupid)
{
	struct path_element *pel;
	tupid_t dt = DOT_DT;
	struct tup_entry *tent;
	const char *top = get_tup_top();

	if(pg->pg_flags & PG_ROOT) {
		list_for_each_entry(pel, &pg->path_list, list) {
			if(is_path_sep(top)) {
				while(*top && is_path_sep(top))
					top++;
				if(name_cmp_n(top, pel->path, pel->len) != 0) {
					/* Directory is outside tup */
					*tupid = -1;
					return 0;
				}
				top += pel->len;
			} else {
				if(tup_db_select_tent_part(dt, pel->path, pel->len, &tent) < 0)
					return -1;
				if(tent == NULL) {
					fprintf(stderr, "tup error: Unable to find tup_entry for node '%.*s' relative to directory %lli\n", pel->len, pel->path, dt);
					tup_db_print(stderr, dt);
					return -1;
				}
				dt = tent->tnode.tupid;
			}
		}
	} else {
		fprintf(stderr, "tup internal error: trying to get_path_tupid() on a pel_group where PG_ROOT isn't set. Is there some funky chdir()ing going on?\n");
		return -1;
	}
	/* If we've reached the end of the full path to the root of the tup
	 * directory, then set our new dt. Otherwise, we are outside of the
	 * tup hierarchy, so set tupid to -1.
	 */
	if(*top == 0) {
		*tupid = dt;
	} else {
		*tupid = -1;
	}
	return 0;
}

int get_path_elements(const char *dir, struct pel_group *pg)
{
	struct path_element *pel;

	init_pel_group(pg);
	if(split_path_elements(dir, pg) < 0)
		return -1;

	if(pg->pg_flags & PG_ROOT) {
		const char *top = get_tup_top();

		do {
			/* Returns are 0 here to indicate file is outside of
			 * .tup
			 */
			if(list_empty(&pg->path_list) || !is_path_sep(top)) {
				pg->pg_flags |= PG_OUTSIDE_TUP;
				return 0;
			}
			while(*top && is_path_sep(top))
				top++;
			pel = list_entry(pg->path_list.next, struct path_element, list);
			if(name_cmp_n(top, pel->path, pel->len) != 0) {
				pg->pg_flags |= PG_OUTSIDE_TUP;
				del_pel_group(pg);
				return 0;
			}
			top += pel->len;

			del_pel(pel, pg);
		} while(*top);
	}
	return 0;
}

static int append_path_elements_tent(struct pel_group *pg, struct tup_entry *tent)
{
	if(tent->tnode.tupid != DOT_DT) {
		if(append_path_elements_tent(pg, tent->parent) < 0)
			return -1;
		if(add_pel(tent->name.s, tent->name.len, pg) < 0)
			return -1;
		pg->num_elements++;
	}
	return 0;
}

int append_path_elements(struct pel_group *pg, tupid_t dt)
{
	struct tup_entry *tent;

	tent = tup_entry_find(dt);
	if(!tent) {
		fprintf(stderr, "tup internal error: tup entry not found for node %lli in append_path_elements\n", dt);
		return -1;
	}
	return append_path_elements_tent(pg, tent);
}

int pg_eq(const struct pel_group *pga, const struct pel_group *pgb)
{
	const struct list_head *la, *lb;
	struct path_element *pela, *pelb;

	la = &pga->path_list;
	lb = &pgb->path_list;
	while(la->next != &pga->path_list && lb->next != &pgb->path_list) {
		pela = list_entry(la->next, struct path_element, list);
		pelb = list_entry(lb->next, struct path_element, list);

		if(pela->len != pelb->len)
			return 0;
		if(name_cmp_n(pela->path, pelb->path, pela->len) != 0)
			return 0;

		la = la->next;
		lb = lb->next;
	}
	if(la->next != &pga->path_list || lb->next != &pgb->path_list)
		return 0;
	return 1;
}

void del_pel(struct path_element *pel, struct pel_group *pg)
{
	list_del(&pel->list);
	free(pel);
	pg->num_elements--;
}

void del_pel_group(struct pel_group *pg)
{
	struct path_element *pel;

	while(!list_empty(&pg->path_list)) {
		pel = list_entry(pg->path_list.prev, struct path_element, list);
		del_pel(pel, pg);
	}
}

void print_pel_group(struct pel_group *pg)
{
	struct path_element *pel;
	printf("Pel[%i, %08x]: ", pg->num_elements, pg->pg_flags);
	if(pg->pg_flags & PG_ROOT) {
		printf("/");
	}
	list_for_each_entry(pel, &pg->path_list, list) {
		printf("%.*s/", pel->len, pel->path);
	}
	printf("\n");
}
