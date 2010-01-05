/* _ATFILE_SOURCE for readlinkat */
#define _ATFILE_SOURCE
#include "fileio.h"
#include "db.h"
#include "compat.h"
#include "config.h"
#include "entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct id_flags {
	tupid_t tupid;
	int flags;
};

static int tup_del_id_type(tupid_t tupid, int type, int force);
static int ghost_to_file(struct tup_entry *tent);

static void (*rmdir_callback)(tupid_t tupid);

int create_name_file(tupid_t dt, const char *file, time_t mtime,
		     struct tup_entry **entry)
{
	if(tup_db_node_insert_tent(dt, file, -1, TUP_NODE_FILE, mtime, entry) < 0)
		return -1;
	if(tup_db_add_create_list(dt) < 0)
		return -1;
	return 0;
}

tupid_t create_command_file(tupid_t dt, const char *cmd)
{
	struct tup_entry *tent;
	tent = tup_db_create_node(dt, cmd, TUP_NODE_CMD);
	if(tent)
		return tent->tnode.tupid;
	return -1;
}

tupid_t create_dir_file(tupid_t dt, const char *path)
{
	struct tup_entry *tent;
	tent = tup_db_create_node(dt, path, TUP_NODE_DIR);
	if(tent)
		return tent->tnode.tupid;
	return -1;
}

tupid_t update_symlink_fileat(tupid_t dt, int dfd, const char *file,
			      time_t mtime, int force)
{
	int rc;
	struct tup_entry *tent;
	struct tup_entry *link_entry;
	tupid_t newsym;
	static char linkname[PATH_MAX];

	if(tup_db_select_tent(dt, file, &tent) < 0)
		return -1;
	if(!tent) {
		if(create_name_file(dt, file, mtime, &tent) < 0)
			return -1;
	} else {
		if(tent->type == TUP_NODE_GHOST) {
			if(ghost_to_file(tent) < 0)
				return -1;
		}
	}

	rc = readlinkat(dfd, file, linkname, sizeof(linkname));
	if(rc < 0) {
		fprintf(stderr, "readlinkat: ");
		perror(file);
		return -1;
	}
	if(rc >= (signed)sizeof(linkname)) {
		fprintf(stderr, "tup error: linkname buffer is too small for the symlink of '%s'\n", file);
		return -1;
	}
	linkname[rc] = 0;

	if(gimme_node_or_make_ghost(dt, linkname, &link_entry) < 0)
		return -1;
	if(link_entry) {
		newsym = link_entry->tnode.tupid;
	} else {
		newsym = -1;
	}

	if(tent->sym != newsym || tent->mtime != mtime || force) {
		if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
			return -1;
	}
	if(tent->sym != newsym) {
		if(tup_db_set_sym(tent, newsym) < 0)
			return -1;
	}
	if(tent->mtime != mtime)
		if(tup_db_set_mtime(tent, mtime) < 0)
			return -1;
	return tent->tnode.tupid;
}

tupid_t tup_file_mod(tupid_t dt, const char *file)
{
	int fd;
	struct stat buf;

	fd = tup_db_open_tupid(dt);
	if(fd < 0)
		return -1;
	if(fstatat(fd, file, &buf, AT_SYMLINK_NOFOLLOW) != 0) {
		fprintf(stderr, "tup error: tup_file_mod() fstatat failed.\n");
		perror(file);
		return -1;
	}
	close(fd);
	return tup_file_mod_mtime(dt, file, buf.st_mtime, 1);
}

tupid_t tup_file_mod_mtime(tupid_t dt, const char *file, time_t mtime,
			   int force)
{
	struct tup_entry *tent;
	int new = 0;
	int modified = 0;

	if(tup_db_select_tent(dt, file, &tent) < 0)
		return -1;

	if(!tent) {
		if(create_name_file(dt, file, mtime, &tent) < 0)
			return -1;
		new = 1;
	} else {
		if(tent->mtime != mtime || force)
			modified = 1;

		if(tent->type == TUP_NODE_GHOST) {
			if(ghost_to_file(tent) < 0)
				return -1;
		} else if(tent->type != TUP_NODE_FILE &&
			  tent->type != TUP_NODE_GENERATED) {
			fprintf(stderr, "tup error: tup_file_mod(%lli, %s) expecting to move a file to the modify_list, but got type: %i\n", dt, file, tent->type);
			return -1;
		}
		if(modified) {
			if(tent->type == TUP_NODE_GENERATED) {
				fprintf(stderr, "tup warning: generated file '%s' was modified outside of tup. This file will be overwritten on the next update, unless the rule that creates it is also removed.\n", file);
				if(tup_db_modify_cmds_by_output(tent->tnode.tupid, NULL) < 0)
					return -1;
			}
			if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
				return -1;

			/* It's possible this is a file that was included by a
			 * Tupfile.  Try to set any dependent directory flags.
			 */
			if(tup_db_set_dependent_dir_flags(tent->tnode.tupid) < 0)
				return -1;

			/* Need to re-parse the Tupfile if it was changed. */
			if(strcmp(file, "Tupfile") == 0) {
				if(tup_db_add_create_list(dt) < 0)
					return -1;
			}

			if(tent->mtime != mtime)
				if(tup_db_set_mtime(tent, mtime) < 0)
					return -1;
		}
	}

	if(new || modified) {
		if(dt == DOT_DT && strcmp(file, TUP_CONFIG) == 0) {
			/* If tup.config was modified, put the @-directory in
			 * the create list so we can import any variables that
			 * have changed.
			 */
			if(tup_db_add_create_list(VAR_DT) < 0)
				return -1;
		}
	}

	return tent->tnode.tupid;
}

static int check_rm_tup_config(struct tup_entry *tent)
{
	if(tent->dt == DOT_DT && strcmp(tent->name.s, TUP_CONFIG) == 0) {
		/* If tup.config was removed, also add the @-directory to the
		 * create list.
		 */
		if(tup_db_add_create_list(VAR_DT) < 0)
			return -1;
	}
	return 0;
}

int tup_file_del(tupid_t dt, const char *file, int len)
{
	struct tup_entry *tent;

	if(tup_db_select_tent_part(dt, file, len, &tent) < 0)
		return -1;
	if(!tent) {
		fprintf(stderr, "[31mError: Trying to delete file '%s', which isn't in .tup/db[0m\n", file);
		return -1;
	}
	if(check_rm_tup_config(tent) < 0)
		return -1;
	return tup_del_id_type(tent->tnode.tupid, tent->type, 0);
}

int tup_file_missing(struct tup_entry *tent)
{
	if(check_rm_tup_config(tent) < 0)
		return -1;
	return tup_del_id_type(tent->tnode.tupid, tent->type, 0);
}

int tup_del_id_force(tupid_t tupid, int type)
{
	return tup_del_id_type(tupid, type, 1);
}

void tup_register_rmdir_callback(void (*callback)(tupid_t tupid))
{
	rmdir_callback = callback;
}

static int tup_del_id_type(tupid_t tupid, int type, int force)
{
	if(type == TUP_NODE_DIR) {
		/* Recurse and kill anything below this dir. Note that
		 * tup_db_delete_dir() calls back to this function.
		 */
		if(tup_db_delete_dir(tupid) < 0)
			return -1;
		if(rmdir_callback)
			rmdir_callback(tupid);
	}

	/* If a file was deleted and it was created by a command, set the
	 * command's flags to modify. For example, if foo.o was deleted, we set
	 * 'gcc -c foo.c -o foo.o' to modify, so it will be re-executed. This
	 * only happens if a file was deleted outside of the parser (!force).
	 */
	if(type == TUP_NODE_GENERATED && !force) {
		int modified = 0;

		if(tup_db_modify_cmds_by_output(tupid, &modified) < 0)
			return -1;
		/* Only display a warning if the command isn't already in the
		 * modify list. It's possible that the command hasn't actually
		 * been executed yet.
		 */
		if(modified == 1)
			fprintf(stderr, "tup warning: generated file ID %lli was deleted outside of tup. This file may be re-created on the next update.\n", tupid);
		/* If we're not forcing the deletion, just return here (the
		 * node won't actually be removed from tup). The fact that the
		 * command is in modify will take care of dependencies, and
		 * we don't want to put the directory back in create (t6036).
		 */
		return 0;
	}

	if(type == TUP_NODE_FILE || type == TUP_NODE_DIR) {
		/* It's possible this is a file that was included by a Tupfile.
		 * Try to set any dependent directory flags.
		 */
		if(tup_db_set_dependent_dir_flags(tupid) < 0)
			return -1;
	}

	if(type == TUP_NODE_FILE || type == TUP_NODE_GENERATED) {
		/* We also have to run any command that used this file as an
		 * input, so we can yell at the user if they haven't already
		 * fixed that command.
		 */
		if(tup_db_modify_cmds_by_input(tupid) < 0)
			return -1;

		if(!force) {
			/* Re-parse the current Tupfile (the updater
			 * automatically parses any dependent directories).
			 */
			if(tup_db_add_dir_create_list(tupid) < 0)
				return -1;
		}
	}
	if(delete_name_file(tupid) < 0)
		return -1;
	return 0;
}

struct tup_entry *get_tent_dt(tupid_t dt, const char *path)
{
	struct path_element *pel = NULL;
	struct tup_entry *tent;

	dt = find_dir_tupid_dt(dt, path, &pel, NULL, 0);
	if(dt < 0)
		return NULL;

	if(pel) {
		if(tup_db_select_tent_part(dt, pel->path, pel->len, &tent) < 0)
			return NULL;
		free(pel);
		if(!tent)
			return NULL;
		if(tup_entry_sym_follow(tent) < 0)
			return NULL;
		while(tent->sym != -1) {
			tent = tent->symlink;
		}
		return tent;
	} else {
		/* We get here if the path list ends up being empty (for
		 * example, if the path is ".")
		 */
		return tup_entry_get(dt);
	}
}

tupid_t find_dir_tupid(const char *dir)
{
	struct tup_entry *tent;

	tent = get_tent_dt(DOT_DT, dir);
	if(!tent)
		return -1;
	return tent->tnode.tupid;
}

tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir,
			  struct path_element **last, struct rb_root *symtree,
			  int sotgv)
{
	struct pel_group pg;
	tupid_t tupid;

	if(get_path_elements(dir, &pg) < 0)
		return -1;

	tupid = find_dir_tupid_dt_pg(dt, &pg, last, NULL, symtree, sotgv);
	return tupid;
}

tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     struct path_element **last,
			     struct list_head *symlist,
			     struct rb_root *symtree, int sotgv)
{
	struct path_element *pel;
	struct tup_entry *tent;

	/* Ignore if the file is hidden or outside of the tup hierarchy */
	if((pg->pg_flags & PG_HIDDEN) || (pg->pg_flags & PG_OUTSIDE_TUP))
		return 0;

	/* The list can be empty if dir is "." or something like "foo/..". In
	 * this case just return dt (the start dir).
	 */
	if(list_empty(&pg->path_list)) {
		if(tup_entry_add(dt, &tent) < 0)
			return -1;
		return dt;
	}

	if(last) {
		pel = list_entry(pg->path_list.prev, struct path_element, list);
		*last = pel;
		list_del(&pel->list);
	} else {
		/* TODO */
		fprintf(stderr, "[31mBork[0m\n");
		exit(1);
	}

	if(pg->pg_flags & PG_ROOT)
		dt = 1;
	if(tup_entry_add(dt, &tent) < 0)
		return -1;

	while(!list_empty(&pg->path_list)) {
		pel = list_entry(pg->path_list.next, struct path_element, list);
		if(pel->len == 2 && pel->path[0] == '.' && pel->path[1] == '.') {
			if(tent->parent == NULL) {
				/* If we're at the top of the tup hierarchy and
				 * trying to go up a level, bail out and return
				 * success since we don't keep track of files
				 * in the great beyond.
				 */
				free(*last);
				*last = NULL;
				del_pel_list(&pg->path_list);
				return 0;
			}
			tent = tent->parent;
		} else {
			tupid_t curdt;

			curdt = tent->tnode.tupid;
			if(tup_db_select_tent_part(curdt, pel->path, pel->len, &tent) < 0)
				return -1;
			if(!tent) {
				/* Secret of the ghost valley! */
				if(sotgv == 0)
					return -1;
				if(tup_db_node_insert_tent(curdt, pel->path, pel->len, TUP_NODE_GHOST, -1, &tent) < 0)
					return -1;
			}
			if(tup_entry_sym_follow(tent) < 0)
				return -1;

			for(; tent->sym != -1; tent = tent->symlink) {
				if(symlist)  {
					tup_entry_list_add(tent, symlist);
				}
				if(symtree) {
					if(tupid_tree_add(symtree, tent->tnode.tupid) < 0)
						return -1;
				}
			}
		}

		del_pel(pel);
	}

	return tent->tnode.tupid;
}

int add_node_to_list(tupid_t dt, struct pel_group *pg, struct list_head *list,
		     int sotgv)
{
	tupid_t new_dt;
	struct path_element *pel = NULL;
	struct tup_entry *tent;

	new_dt = find_dir_tupid_dt_pg(dt, pg, &pel, list, NULL, sotgv);
	if(new_dt < 0)
		return -1;
	if(new_dt == 0) {
		return 0;
	}
	if(pel == NULL) {
		tent = tup_entry_get(new_dt);
		if(!tent)
			return -1;
		tup_entry_list_add(tent, list);
		return 0;
	}

	if(tup_db_select_tent_part(new_dt, pel->path, pel->len, &tent) < 0)
		return -1;
	if(!tent) {
		if(sotgv) {
			if(tup_db_node_insert_tent(new_dt, pel->path, pel->len, TUP_NODE_GHOST, -1, &tent) < 0) {
				fprintf(stderr, "Error: Node '%.*s' doesn't exist in directory %lli, and no luck creating a ghost node there.\n", pel->len, pel->path, new_dt);
				return -1;
			}
		} else {
			fprintf(stderr, "tup error: Expected node '%.*s' to be in directory %lli, but it is not there.\n", pel->len, pel->path, new_dt);
			tup_db_print(stderr, new_dt);
			return -1;
		}
	}
	free(pel);

	if(tup_entry_sym_follow(tent) < 0)
		return -1;
	for(; tent->sym != -1; tent = tent->symlink) {
		tup_entry_list_add(tent, list);
	}
	tup_entry_list_add(tent, list);

	return 0;
}

int gimme_node_or_make_ghost(tupid_t dt, const char *name,
			     struct tup_entry **entry)
{
	tupid_t new_dt;
	struct path_element *pel = NULL;

	new_dt = find_dir_tupid_dt(dt, name, &pel, NULL, 1);
	if(new_dt < 0)
		return -1;

	if(new_dt == 0) {
		*entry = NULL;
		return 0;
	}
	if(pel == NULL) {
		*entry = tup_entry_get(new_dt);
		return 0;
	}

	if(tup_db_select_tent_part(new_dt, pel->path, pel->len, entry) < 0)
		return -1;
	if(!*entry) {
		if(tup_db_node_insert_tent(new_dt, pel->path, pel->len, TUP_NODE_GHOST, -1, entry) < 0) {
			fprintf(stderr, "Error: Node '%.*s' doesn't exist in directory %lli, and no luck creating a ghost node there.\n", pel->len, pel->path, new_dt);
			return -1;
		}
	}
	free(pel);

	return 0;
}

int get_path_elements(const char *dir, struct pel_group *pg)
{
	struct path_element *pel;
	const char *p = dir;
	int num_elements = 0;

	pg->pg_flags = 0;
	INIT_LIST_HEAD(&pg->path_list);

	if(dir[0] == '/')
		pg->pg_flags |= PG_ROOT;

	while(1) {
		const char *path;
		int len;
		while(*p && *p == '/') {
			p++;
		}
		if(!*p)
			break;
		path = p;
		while(*p && *p != '/') {
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
				if(num_elements) {
					pel = list_entry(pg->path_list.prev, struct path_element, list);
					num_elements--;
					del_pel(pel);
					continue;
				}
				/* Don't set num_elements, since a ".." path
				 * can't be deleted by a subsequent ".."
				 */
				goto skip_num_elements;
			} else {
				/* Ignore hidden paths */
				del_pel_list(&pg->path_list);
				pg->pg_flags |= PG_HIDDEN;
				return 0;
			}
		}

		num_elements++;
skip_num_elements:

		pel = malloc(sizeof *pel);
		if(!pel) {
			perror("malloc");
			return -1;
		}
		pel->path = path;
		pel->len = len;
		list_add_tail(&pel->list, &pg->path_list);
	}

	if(pg->pg_flags & PG_ROOT) {
		const char *top = get_tup_top();

		do {
			/* Returns are 0 here to indicate file is outside of
			 * .tup
			 */
			if(list_empty(&pg->path_list) || top[0] != '/') {
				pg->pg_flags |= PG_OUTSIDE_TUP;
				return 0;
			}
			top++;
			pel = list_entry(pg->path_list.next, struct path_element, list);
			if(strncmp(top, pel->path, pel->len) != 0) {
				pg->pg_flags |= PG_OUTSIDE_TUP;
				del_pel_list(&pg->path_list);
				return 0;
			}
			top += pel->len;

			del_pel(pel);
		} while(*top);
	}
	return 0;
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
		if(strncmp(pela->path, pelb->path, pela->len) != 0)
			return 0;

		la = la->next;
		lb = lb->next;
	}
	if(la->next != &pga->path_list || lb->next != &pgb->path_list)
		return 0;
	return 1;
}

void del_pel(struct path_element *pel)
{
	list_del(&pel->list);
	free(pel);
}

void del_pel_list(struct list_head *list)
{
	struct path_element *pel;

	while(!list_empty(list)) {
		pel = list_entry(list->prev, struct path_element, list);
		del_pel(pel);
	}
}

static int ghost_to_file(struct tup_entry *tent)
{
	if(tup_db_set_type(tent, TUP_NODE_FILE) < 0)
		return -1;
	if(tup_db_add_create_list(tent->dt) < 0)
		return -1;
	if(tup_db_add_modify_list(tent->tnode.tupid) < 0)
		return -1;
	return 0;
}
