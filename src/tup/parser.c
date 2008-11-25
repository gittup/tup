#define _ATFILE_SOURCE
#include "parser.h"
#include "list.h"
#include "flist.h"
#include "fileio.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <sys/stat.h>

struct rule {
	struct list_head list;
	int foreach;
	char *input_pattern;
	char *output_pattern;
	char *command;
};

struct buf {
	char *s;
	int len;
};

static int fslurp(int fd, struct buf *b);
static int execute_tupfile(struct buf *b, const char *dir, tupid_t tupid);
static int execute_rules(struct list_head *rules, const char *dir, tupid_t dt);
static int do_rule(struct rule *r, const char *filename, const char *dir,
		   tupid_t dt);
static char *tup_printf(const char *cmd, const char *dir, const char *filename);

int parser_create(const char *dir, tupid_t tupid)
{
	int dfd;
	int fd;
	int rc = 0;
	struct buf b;

	dfd = open(dir, O_RDONLY);
	if(dfd < 0) {
		perror(dir);
		return -1;
	}
	fd = openat(dfd, "Tupfile", O_RDONLY);
	if(fd < 0)
		goto out_close_dir;

	if((rc = fslurp(fd, &b)) < 0) {
		goto out_close_dir;
	}
	rc = execute_tupfile(&b, dir, tupid);
	free(b.s);

out_close_dir:
	close(dfd);
	return rc;
}

static int fslurp(int fd, struct buf *b)
{
	struct stat st;
	char *tmp;
	int rc;

	if(fstat(fd, &st) < 0) {
		return -1;
	}

	tmp = malloc(st.st_size);
	if(!tmp) {
		return -1;
	}

	rc = read(fd, tmp, st.st_size);
	if(rc < 0) {
		goto err_out;
	}
	if(rc != st.st_size) {
		errno = EIO;
		goto err_out;
	}

	b->s = tmp;
	b->len = st.st_size;
	return 0;

err_out:
	free(tmp);
	return -1;
}

static int execute_tupfile(struct buf *b, const char *dir, tupid_t tupid)
{
	char *p, *e;
	char *input, *cmd, *output;
	char *ie, *ce, *oe;
	char *line;
	struct rule *r;
	int rc;
	LIST_HEAD(rules);

	p = b->s;
	e = b->s + b->len;

	while(p < e) {
		line = p;

		input = p;
		while(*input == ' ')
			input++;
		p = strstr(p, ">>");
		if(!p)
			goto syntax_error;
		ie = p - 1;
		while(*ie == ' ')
			ie--;
		p += 2;
		cmd = p;
		while(*cmd == ' ')
			cmd++;
		p = strstr(p, ">>");
		if(!p)
			goto syntax_error;
		ce = p - 1;
		while(*ce == ' ')
			ce--;
		p += 2;
		output = p;
		while(*output == ' ')
			output++;
		p = strstr(p, "\n");
		if(!p)
			goto syntax_error;
		oe = p - 1;
		while(*oe == ' ')
			oe--;
		ie[1] = 0;
		ce[1] = 0;
		oe[1] = 0;
		p++;

		r = malloc(sizeof *r);
		if(!r) {
			perror("malloc");
			return -1;
		}
		if(strncmp(input, "foreach ", 8) == 0) {
			r->input_pattern = input + 8;
			r->foreach = 1;
		} else {
			r->input_pattern = input;
			r->foreach = 0;
		}
		r->output_pattern = output;
		r->command = cmd;

		list_add(&r->list, &rules);
	}

	rc = execute_rules(&rules, dir, tupid);
	while(!list_empty(&rules)) {
		r = list_entry(rules.next, struct rule, list);
		list_del(&r->list);
		free(r);
	}
	return rc;

syntax_error:
	fprintf(stderr, "Syntax error parsing %s/Tupfile\nLine was: %s",
		dir, line);
	return -1;
}

static int execute_rules(struct list_head *rules, const char *dir, tupid_t dt)
{
	struct rule *r;
	struct flist f = {0, 0, 0};

	flist_foreach(&f, dir) {
		list_for_each_entry(r, rules, list) {
			if(f.filename[0] == '.')
				continue;
			if(do_rule(r, f.filename, dir, dt) < 0)
				return -1;
		}
	}
	return 0;
}

static int do_rule(struct rule *r, const char *filename, const char *dir,
		   tupid_t dt)
{
	int flags = FNM_PATHNAME | FNM_PERIOD;
	char *cmd;
	char *output;
	tupid_t cmd_id;
	tupid_t in_id;
	tupid_t out_id;
	static char cname[PATH_MAX];

	if(canonicalize2(dir, filename, cname, sizeof(cname)) < 0)
		return -1;
	in_id = create_name_file(cname);
	if(in_id < 0)
		return -1;
	if(fnmatch(r->input_pattern, filename, flags) == 0) {
		if(r->foreach) {
			cmd = tup_printf(r->command, dir, filename);
			if(!cmd)
				return -1;
			cmd_id = create_command_file(cmd);
			if(cmd_id < 0)
				return -1;
			if(tup_db_create_cmdlink(dt, cmd_id) < 0)
				return -1;
			printf("Match[%lli]: %s, %s\n", dt, r->input_pattern,
			       filename);
			printf("Command: %s\n", cmd);
			free(cmd);

			output = tup_printf(r->output_pattern, dir, filename);
			out_id = create_name_file(output);
			if(out_id < 0)
				return -1;

			if(tup_db_create_link(in_id, cmd_id) < 0)
				return -1;
			if(tup_db_create_link(cmd_id, out_id) < 0)
				return -1;

			printf("Output: %s\n", output);
			free(output);
		}
	}
	return 0;
}

static char *tup_printf(const char *cmd, const char *dir, const char *filename)
{
	char *s;
	int x;
	const char *p;
	const char *next;
	int clen = strlen(cmd);
	int dlen = strlen(dir);
	int flen = strlen(filename);
	int extlessflen;

	extlessflen = flen - 1;
	while(extlessflen > 0 && filename[extlessflen] != '.') {
		extlessflen--;
	}

	printf("File: '%s', len = %i, extlessflen: %i\n", filename, flen, extlessflen);

	p = cmd;
	while((p = strchr(p, '$')) !=  NULL) {
		p++;
		if(*p == 'p') {
			clen += dlen + 1 + flen;
		} else if(*p == 'P') {
			clen += dlen + 1 + extlessflen;
		}
	}

	s = malloc(clen + 1);
	if(!s) {
		perror("malloc");
		return NULL;
	}

	p = cmd;
	x = 0;
	while((next = strchr(p, '$')) !=  NULL) {
		memcpy(&s[x], p, next-p);
		x += next-p;

		next++;
		p = next + 1;
		if(*next == 'p') {
			if(dir[0] != '.') {
				memcpy(&s[x], dir, dlen);
				x += dlen;
				s[x] = '/';
				x++;
			}
			memcpy(&s[x], filename, flen);
			x += flen;
		} else if(*next == 'P') {
			if(dir[0] != '.') {
				memcpy(&s[x], dir, dlen);
				x += dlen;
				s[x] = '/';
				x++;
			}
			memcpy(&s[x], filename, extlessflen);
			x += extlessflen;
		}
	}
	strcpy(&s[x], p);
	return s;
}
