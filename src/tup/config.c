#include "config.h"
#include "compat.h"
#include "db.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static char tup_wd[PATH_MAX];
static int tup_wd_offset;
static int tup_top_len;
static int tup_sub_len;
static tupid_t tup_sub_dir_dt = -1;
static int top_fd = -1;

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
	top_fd = open(".", O_RDONLY);
	if(top_fd < 0) {
		perror(".");
		return -1;
	}
	return 0;
}

tupid_t get_sub_dir_dt(void)
{
	if(tup_sub_dir_dt < 0) {
		tup_sub_dir_dt = find_dir_tupid(get_sub_dir());
		if(tup_sub_dir_dt < 0) {
			fprintf(stderr, "Error: Unable to find tupid for working directory: '%s'\n", get_sub_dir());
		}
	}
	return tup_sub_dir_dt;
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

int tup_top_fd(void)
{
	return top_fd;
}
