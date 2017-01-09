/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

#include "pel_group.h"
#include "db.h"
#include "config.h"
#include "entry.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pel_ignored(const char *path, int len)
{
	if(len < 0)
		len = strlen(path);
	if(len == 1 && strncmp(path, ".", 1) == 0)
		return 1;
	if(len == 2 && strncmp(path, "..", 2) == 0)
		return 1;
	if(len == 4 && strncmp(path, ".tup", 4) == 0)
		return 1;
	if(len == 4 && strncmp(path, ".git", 4) == 0)
		return 1;
	if(len == 3 && strncmp(path, ".hg", 3) == 0)
		return 1;
	if(len == 4 && strncmp(path, ".bzr", 4) == 0)
		return 1;
	if(len == 4 && strncmp(path, ".svn", 4) == 0)
		return 1;
	/* See also fuse_fs.c:is_hidden() */
	return 0;
}

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
	TAILQ_INSERT_TAIL(&pg->path_list, pel, list);
	return 0;
}

void init_pel_group(struct pel_group *pg)
{
	pg->pg_flags = 0;
	pg->num_elements = 0;
	TAILQ_INIT(&pg->path_list);
}

static int split_path_elements(const char *dir, struct pel_group *pg)
{
	struct path_element *pel;
	const char *p = dir;

	if(is_full_path(dir)) {
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
					pel = TAILQ_LAST(&pg->path_list, path_element_head);
					del_pel(pel, pg);
					continue;
				}
				/* Don't set num_elements, since a ".." path
				 * can't be deleted by a subsequent ".."
				 */
				goto skip_num_elements;
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
		TAILQ_FOREACH(pel, &pg->path_list, list) {
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

int get_path_elements(const char *path, struct pel_group *pg)
{
	struct path_element *pel;

	if(!path) {
		fprintf(stderr, "tup internal error: 'path' is NULL in get_path_elements()\n");
		return -1;
	}

	init_pel_group(pg);
	if(split_path_elements(path, pg) < 0)
		return -1;

	if(pg->pg_flags & PG_ROOT) {
		const char *top = get_tup_top();
		int num_pels = 0;

		TAILQ_FOREACH(pel, &pg->path_list, list) {
			while(*top && is_path_sep(top))
				top++;
			if(name_cmp_n(top, pel->path, pel->len) != 0) {
				break;
			}
			top += pel->len;
			num_pels++;
		}
		if(*top) {
			pg->pg_flags |= PG_OUTSIDE_TUP;
		} else {
			/* If we're inside tup, remove the part of the path up to where the
			 * .tup hierarchy starts.
			 */
			int x;
			for(x=0; x<num_pels; x++) {
				pel = TAILQ_FIRST(&pg->path_list);
				del_pel(pel, pg);
			}
		}
	}

	TAILQ_FOREACH(pel, &pg->path_list, list) {
		if(pel->path[0] == '.') {
			if(pel->len == 2 && strncmp(pel->path, "..", 2) == 0) {
				/* .. paths are ignored */
			} else if(pel_ignored(pel->path,  pel->len)) {
				/* Hidden paths have special treatment in tup */
				pg->pg_flags |= PG_HIDDEN;
				break;
			}
		}
	}

	if(!TAILQ_EMPTY(&pg->path_list)) {
		pel = TAILQ_LAST(&pg->path_list, path_element_head);
		if(pel->path[0] == '<' && pel->path[pel->len-1] == '>') {
			pg->pg_flags |= PG_GROUP;
		}
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
	struct path_element *pela, *pelb;

	pela = TAILQ_FIRST(&pga->path_list);
	pelb = TAILQ_FIRST(&pgb->path_list);
	while(pela != NULL && pelb != NULL) {
		if(pela->len != pelb->len)
			return 0;
		if(name_cmp_n(pela->path, pelb->path, pela->len) != 0)
			return 0;

		pela = TAILQ_NEXT(pela, list);
		pelb = TAILQ_NEXT(pelb, list);
	}
	if(pela != NULL || pelb != NULL)
		return 0;
	return 1;
}

void del_pel(struct path_element *pel, struct pel_group *pg)
{
	TAILQ_REMOVE(&pg->path_list, pel, list);
	free(pel);
	pg->num_elements--;
}

void del_pel_group(struct pel_group *pg)
{
	struct path_element *pel;

	while(!TAILQ_EMPTY(&pg->path_list)) {
		pel = TAILQ_LAST(&pg->path_list, path_element_head);
		del_pel(pel, pg);
	}
}

void print_pel_group(struct pel_group *pg)
{
	struct path_element *pel;
	int slash = 0;
	printf("Pel[%i, %08x]: ", pg->num_elements, pg->pg_flags);
	if((pg->pg_flags & PG_ROOT) && (pg->pg_flags & PG_OUTSIDE_TUP)) {
		slash = 1;
	}
	TAILQ_FOREACH(pel, &pg->path_list, list) {
		if(slash)
			printf("/");
		slash = 1;
		printf("%.*s", pel->len, pel->path);
	}
	printf("\n");
}
