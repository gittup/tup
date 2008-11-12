#include "config.h"
#include "compat.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static char tup_wd[PATH_MAX];
static int tup_wd_offset;
static int tup_top_len;
static int tup_sub_len;
static int config_get_int_cb(void *arg, int argc, char **argv, char **col);

static int config_get_int_cb(void *arg, int argc, char **argv, char **col)
{
	int x;
	int *iptr = arg;

	for(x=0; x<argc; x++) {
		if(strcmp(col[x], "rval") == 0)
			*iptr = atoi(argv[x]);
	}
	return 0;
}

int config_get_int(const char *lval)
{
	int x = 0;
	if(tup_db_select(config_get_int_cb, &x,
			 "select rval from config where lval='%q'", lval) != 0)
		return -1;
	return x;
}

int config_set_int(const char *lval, int x)
{
	if(tup_db_exec("delete from config where lval='%q'", lval) != 0)
		return -1;
	if(tup_db_exec("insert into config values('%q', %i)", lval, x) != 0)
		return -1;
	return 0;
}

int find_tup_dir(void)
{
	struct stat st;

	if(getcwd(tup_wd, sizeof(tup_wd)) == NULL) {
		perror("getcwd");
		return -1;
	}

	tup_top_len = strlen(tup_wd);
	tup_sub_len = 0;
	for(;;) {
		if(stat(".tup", &st) == 0 && S_ISDIR(st.st_mode)) {
			tup_wd_offset = tup_top_len;
			while(tup_wd[tup_wd_offset] == '/') {
				tup_wd_offset++;
				tup_sub_len--;
			}
			tup_wd[tup_top_len] = 0;
			break;
		}
		chdir("..");
		while(tup_top_len > 0) {
			tup_top_len--;
			tup_sub_len++;
			if(tup_wd[tup_top_len] == '/') {
				break;
			}
		}
		if(!tup_top_len) {
			fprintf(stderr, "No .tup directory found. Run 'tup "
				"init' to create the dependency filesystem.\n");
			return -1;
		}
	}
	return 0;
}

const char *get_tup_top(void)
{
	return tup_wd;
}

int get_tup_top_len(void)
{
	return tup_top_len;
}

const char *get_sub_dir(void)
{
	if(tup_wd[tup_wd_offset])
		return tup_wd + tup_wd_offset;
	return ".";
}

int get_sub_dir_len(void)
{
	return tup_sub_len;
}
