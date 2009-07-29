/* _ATFILE_SOURCE for readlinkat */
#define _ATFILE_SOURCE
#include "fileio.h"
#include "db.h"
#include "compat.h"
#include "config.h"
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

static int ghost_to_file(struct db_node *dbn);

tupid_t create_name_file(tupid_t dt, const char *file, time_t mtime)
{
	tupid_t tupid;

	tupid = tup_db_node_insert(dt, file, -1, TUP_NODE_FILE, mtime);
	if(tupid < 0)
		return -1;
	if(tup_db_add_create_list(dt) < 0)
		return -1;
	return tupid;
}

tupid_t create_command_file(tupid_t dt, const char *cmd)
{
	return tup_db_create_node(dt, cmd, TUP_NODE_CMD);
}

tupid_t create_dir_file(tupid_t dt, const char *path)
{
	return tup_db_create_node(dt, path, TUP_NODE_DIR);
}

tupid_t update_symlink_fileat(tupid_t dt, int dfd, const char *file,
			      time_t mtime, int force)
{
	int rc;
	struct db_node dbn;
	struct db_node link_dbn;
	tupid_t link_dt;
	static char linkname[PATH_MAX];

	if(tup_db_select_dbn(dt, file, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0) {
		dbn.tupid = create_name_file(dt, file, mtime);
		if(dbn.tupid < 0)
			return -1;
	} else {
		if(dbn.type == TUP_NODE_GHOST) {
			if(ghost_to_file(&dbn) < 0)
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

	link_dt = find_dir_tupid_dt(dt, linkname, &file, NULL, 1);
	if(link_dt <= 0) {
		fprintf(stderr, "Error: Unable to find directory ID for '%s' in update_symlink_file()\n", linkname);
		return -1;
	}
	if(tup_db_select_dbn(link_dt, file, &link_dbn) < 0)
		return -1;
	if(link_dbn.tupid < 0) {
		link_dbn.tupid = tup_db_node_insert(link_dt, file, -1, TUP_NODE_GHOST, -1);
		if(link_dbn.tupid < 0) {
			fprintf(stderr, "Error: Node '%s' doesn't exist in directory %lli, and no luck creating a ghost node there.\n", file, link_dt);
			return -1;
		}
	}
	if(dbn.sym != link_dbn.tupid) {
		if(tup_db_set_sym(dbn.tupid, link_dbn.tupid) < 0)
			return -1;
	}
	if(dbn.sym != link_dbn.tupid || dbn.mtime != mtime || force) {
		if(tup_db_add_modify_list(dbn.tupid) < 0)
			return -1;
	}
	if(dbn.mtime != mtime)
		if(tup_db_set_mtime(dbn.tupid, mtime) < 0)
			return -1;
	return dbn.tupid;
}

tupid_t create_var_file(const char *var, const char *value)
{
	int rc;
	struct db_node dbn;

	if(tup_db_select_dbn(VAR_DT, var, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0) {
		dbn.tupid = tup_db_create_node(VAR_DT, var, TUP_NODE_VAR);
		if(dbn.tupid < 0)
			return -1;
	} else {
		if(dbn.type == TUP_NODE_VAR) {
			char *orig_value;
			if(tup_db_get_var_id_alloc(dbn.tupid, &orig_value) < 0)
				return -1;
			rc = strcmp(orig_value, value);
			free(orig_value);
			/* If the value hasn't changed, just make sure it isn't
			 * scheduled for deletion.
			 */
			if(rc == 0) {
				if(tup_db_unflag_tmp(dbn.tupid) < 0)
					return -1;
				return 0;
			}
		} else if(dbn.type == TUP_NODE_GHOST) {
			if(tup_db_set_type(dbn.tupid, TUP_NODE_VAR) < 0)
				return -1;
		} else {
			fprintf(stderr, "tup error: Unexpected node type %i in create_var_file(). Should be TUP_NODE_VAR or TUP_NODE_GHOST.\n", dbn.type);
			return -1;
		}

		if(tup_db_add_create_list(dbn.tupid) < 0)
			return -1;
		if(tup_db_add_modify_list(dbn.tupid) < 0)
			return -1;
		if(tup_db_unflag_tmp(dbn.tupid) < 0)
			return -1;
	}
	return tup_db_set_var(dbn.tupid, value);
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
	struct db_node dbn;
	int modified = 0;

	if(tup_db_select_dbn(dt, file, &dbn) < 0)
		return -1;

	if(dbn.mtime != mtime || force)
		modified = 1;

	/* Need to re-parse the Tupfile if it was changed. */
	if(modified) {
		if(strcmp(file, "Tupfile") == 0) {
			if(tup_db_add_create_list(dt) < 0)
				return -1;
		}
	}

	if(dbn.tupid < 0) {
		dbn.tupid = create_name_file(dt, file, mtime);
		if(dbn.tupid < 0)
			return -1;
	} else {
		if(dbn.type == TUP_NODE_GHOST) {
			if(ghost_to_file(&dbn) < 0)
				return -1;
		} else if(dbn.type != TUP_NODE_FILE &&
			  dbn.type != TUP_NODE_GENERATED) {
			fprintf(stderr, "tup error: tup_file_mod() expecting to move a file to the modify_list, but got type: %i\n", dbn.type);
			return -1;
		}
		if(modified) {
			if(dbn.type == TUP_NODE_GENERATED) {
				fprintf(stderr, "tup warning: generated file '%s' was modified outside of tup. This file may be overwritten on the next update.\n", file);
				if(tup_db_modify_cmds_by_output(dbn.tupid, NULL) < 0)
					return -1;
			}
			if(tup_db_add_modify_list(dbn.tupid) < 0)
				return -1;

			/* It's possible this is a file that was included by a
			 * Tupfile.  Try to set any dependent directory flags.
			 */
			if(tup_db_set_dependent_dir_flags(dbn.tupid) < 0)
				return -1;
			if(dbn.mtime != mtime)
				if(tup_db_set_mtime(dbn.tupid, mtime) < 0)
					return -1;
		}
		if(tup_db_unflag_delete(dbn.tupid) < 0)
			return -1;
	}
	return dbn.tupid;
}

int tup_file_del(tupid_t dt, const char *file)
{
	struct db_node dbn;

	if(tup_db_select_dbn(dt, file, &dbn) < 0)
		return -1;
	if(dbn.tupid < 0) {
		fprintf(stderr, "[31mError: Trying to delete file '%s', which isn't in .tup/db[0m\n", file);
		return -1;
	}
	return tup_del_id(dbn.tupid, dbn.dt, dbn.sym, dbn.type);
}

int tup_del_id(tupid_t tupid, tupid_t dt, tupid_t sym, int type)
{
	if(type == TUP_NODE_DIR) {
		/* Directories are pretty simple, but we need to recurse and
		 * kill anything underneath the diretory as well.
		 */
		if(tup_db_delete_dir(tupid) < 0)
			return -1;
		if(delete_name_file(tupid, dt, sym) < 0)
			return -1;
		return 0;
	}
	/* If a file was deleted and it was created by a command, set the
	 * command's flags to modify. For example, if foo.o was deleted, we set
	 * 'gcc -c foo.c -o foo.o' to modify, so it will be re-executed.
	 *
	 * This is really just to mimic what people would expect from make.
	 * Randomly deleting object files is pretty stupid.
	 */
	if(type == TUP_NODE_GENERATED) {
		int modified = 0;
		if(tup_db_modify_cmds_by_output(tupid, &modified) < 0)
			return -1;
		/* Only display a warning if the command isn't already in the
		 * modify list. It's possible that the command hasn't actually
		 * been executed yet.
		 */
		if(modified == 1)
			fprintf(stderr, "tup warning: generated file ID %lli was deleted outside of tup. This file may be re-created on the next update.\n", tupid);
	}

	/* We also have to run any command that used this file as an input, so
	 * we can yell at the user if they haven't already fixed that command.
	 */
	if(tup_db_modify_cmds_by_input(tupid) < 0)
		return -1;

	/* Re-parse the current Tupfile (the updater automatically parses any
	 * dependent directories).
	 */
	if(tup_db_add_create_list(dt) < 0)
		return -1;

	/* It's possible this is a file that was included by a Tupfile.  Try to
	 * set any dependent directory flags.
	 */
	if(tup_db_set_dependent_dir_flags(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(delete_name_file(tupid, dt, sym) < 0)
		return -1;
	return 0;
}

tupid_t get_dbn_dt(tupid_t dt, const char *path, struct db_node *dbn,
		   struct list_head *symlist)
{
	const char *file = NULL;

	dbn->tupid = -1;

	dt = find_dir_tupid_dt(dt, path, &file, symlist, 0);
	if(dt < 0)
		return -1;

	if(file) {
		if(tup_db_select_dbn(dt, file, dbn) < 0)
			return -1;
		if(sym_follow(dbn, symlist) < 0)
			return -1;
		return dbn->tupid;
	} else {
		/* We get here if the path list ends up being empty (for
		 * example, if the path is ".")
		 */
		if(tup_db_select_dbn_by_id(dt, dbn) < 0)
			return -1;
		dbn->name = path;
		return dt;
	}
}

tupid_t get_dbn_dt_pg(tupid_t dt, struct pel_group *pg, struct db_node *dbn,
		      struct list_head *symlist)
{
	const char *file = NULL;

	dbn->tupid = -1;
	dt = find_dir_tupid_dt_pg(dt, pg, &file, symlist, 0);
	if(dt < 0)
		return -1;
	/* File hidden from tup */
	if(dt == 0)
		return 0;

	if(file) {
		if(tup_db_select_dbn(dt, file, dbn) < 0)
			return -1;
		if(sym_follow(dbn, symlist) < 0)
			return -1;
		return dbn->tupid;
	} else {
		fprintf(stderr, "[31mtup internal error: get_dbn_dt_pg() didn't get a final file pointer in find_dir_tupid_dt_pg()[0m\n");
		return -1;
	}
}

tupid_t find_dir_tupid(const char *dir)
{
	struct db_node dbn;

	return get_dbn_dt(DOT_DT, dir, &dbn, NULL);
}

tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir, const char **last,
			  struct list_head *symlist, int sotgv)
{
	struct pel_group pg;
	tupid_t tupid;

	if(get_path_elements(dir, &pg) < 0)
		return -1;

	tupid = find_dir_tupid_dt_pg(dt, &pg, last, symlist, sotgv);
	return tupid;
}

tupid_t find_dir_tupid_dt_pg(tupid_t dt, struct pel_group *pg,
			     const char **last, struct list_head *symlist,
			     int sotgv)
{
	struct path_element *pel;

	/* Ignore if the file is hidden or outside of the tup hierarchy */
	if(pg->pg_flags)
		return 0;

	/* The list can be empty if dir is "." or something like "foo/..". In
	 * this case just return dt (the start dir).
	 */
	if(list_empty(&pg->path_list))
		return dt;

	if(last) {
		pel = list_entry(pg->path_list.prev, struct path_element, list);
		*last = pel->path;
		del_pel(pel);
	} else {
		/* TODO */
		fprintf(stderr, "[31mBork[0m\n");
		exit(1);
	}

	while(!list_empty(&pg->path_list)) {
		struct db_node dbn;

		pel = list_entry(pg->path_list.next, struct path_element, list);
		if(pel->len == 2 && pel->path[0] == '.' && pel->path[1] == '.') {
			if(dt == 0) {
				/* If we're at the top of the tup hierarchy and
				 * trying to go up a level, bail out and return
				 * success since we don't keep track of files
				 * in the great beyond.
				 */
				return 0;
			}
			dt = tup_db_parent(dt);
			if(dt < 0)
				return -1;
		} else {
			if(tup_db_select_dbn_part(dt, pel->path, pel->len, &dbn) < 0)
				return -1;
			if(dbn.tupid < 0) {
				/* Secret of the ghost valley! */
				if(sotgv == 0)
					return -1;
				dbn.tupid = tup_db_node_insert(dt, pel->path, pel->len, TUP_NODE_GHOST, -1);
				if(dbn.tupid < 0)
					return -1;
				dbn.sym = -1;
			}
			if(sym_follow(&dbn, symlist) < 0)
				return -1;
			dt = dbn.tupid;
		}

		del_pel(pel);
	}

	return dt;
}

int get_path_elements(const char *dir, struct pel_group *pg)
{
	struct path_element *pel;
	const char *p = dir;
	int num_elements = 0;
	int is_root;

	if(dir[0] == '/')
		is_root = 1;
	else
		is_root = 0;
	pg->pg_flags = 0;
	INIT_LIST_HEAD(&pg->path_list);

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

	if(is_root) {
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

int sym_follow(struct db_node *dbn, struct list_head *symlist)
{
	while(dbn->sym != -1) {
		if(symlist) {
			struct half_entry *he;
			he = malloc(sizeof *he);
			if(!he) {
				perror("malloc");
				return -1;
			}
			he->tupid = dbn->tupid;
			he->dt = dbn->dt;
			he->sym = dbn->sym;
			he->type = dbn->type;
			list_add(&he->list, symlist);
		}

		if(tup_db_select_dbn_by_id(dbn->sym, dbn) < 0)
			return -1;
	}
	return 0;
}

static int ghost_to_file(struct db_node *dbn)
{
	if(tup_db_set_type(dbn->tupid, TUP_NODE_FILE) < 0)
		return -1;
	if(tup_db_add_create_list(dbn->dt) < 0)
		return -1;
	if(tup_db_add_modify_list(dbn->tupid) < 0)
		return -1;
	dbn->type = TUP_NODE_FILE;
	return 0;
}
