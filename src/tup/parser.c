#define _GNU_SOURCE
#define _ATFILE_SOURCE
#include "parser.h"
#include "list.h"
#include "flist.h"
#include "fileio.h"
#include "db.h"
#include "vardb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <sys/stat.h>

struct name_list {
	struct list_head entries;
	int num_entries;
	int totlen;
	int extlesstotlen;
	tupid_t dt;
};

struct name_list_entry {
	struct list_head list;
	char *path;
	int len;
	int extlesslen;
	tupid_t tupid;
};

struct rule {
	struct list_head list;
	int foreach;
	char *input_pattern;
	char *output_pattern;
	char *command;
	struct name_list namelist;
};

struct buf {
	char *s;
	int len;
};

static int fslurp(int fd, struct buf *b);
static int execute_tupfile(struct buf *b, int dfd, tupid_t tupid);
static int parse_rule(char *p, struct list_head *rules);
static int execute_rules(struct list_head *rules, int dfd, tupid_t dt);
static int file_match(struct rule *r, const char *filename, tupid_t dt);
static int do_rule(struct rule *r, struct name_list *nl, tupid_t dt);
static char *tup_printf(const char *cmd, struct name_list *nl);

int parser_create(tupid_t tupid)
{
	int dfd;
	int fd;
	int rc = 0;
	struct buf b;

	dfd = tup_db_opendir(tupid);
	if(dfd < 0)
		return -1;
	fd = openat(dfd, "Tupfile", O_RDONLY);
	if(fd < 0)
		goto out_close_dir;

	if((rc = fslurp(fd, &b)) < 0) {
		goto out_close_file;
	}
	rc = execute_tupfile(&b, dfd, tupid);
	free(b.s);
out_close_file:
	close(fd);

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

static int execute_tupfile(struct buf *b, int dfd, tupid_t tupid)
{
	char *p, *e;
	char *line;
	int rc;
	struct rule *r;
	struct vardb vdb;
	LIST_HEAD(rules);

	if(vardb_init(&vdb) < 0)
		return -1;

	p = b->s;
	e = b->s + b->len;

	while(p < e) {
		char *nl;
		while((*p == ' ' || *p == '\t' || *p == '\n') && p < e)
			p++;

		line = p;
		nl = strchr(p, '\n');
		if(!nl)
			goto syntax_error;
		*nl = 0;
		if(line[0] == '#') {
			/* Skip comments */
		} else if(line[0] == ':') {
			if(parse_rule(p+1, &rules) < 0)
				goto syntax_error;
		} else {
			char *eq;
			char *value;
			int append;

			/* Find the += or = sign, and point value to the start
			 * of the string after that op.
			 */
			eq = strstr(p, "+=");
			if(eq) {
				value = eq + 2;
				append = 1;
			} else {
				eq = strchr(p, '=');
				if(!eq)
					goto syntax_error;
				append = 0;
				value = eq + 1;
			}

			/* End the lval with a 0, then space-delete the end
			 * of the variable name and the beginning of the value.
			 */
			*eq = 0;
			while(*value == ' ' && *value != 0)
				value++;
			eq--;
			while(*eq == ' ' && eq > p) {
				*eq = 0;
				eq--;
			}

			if(append)
				vardb_append(&vdb, p, value);
			else
				vardb_set(&vdb, p, value);
		}
		p = nl + 1;
	}

	rc = execute_rules(&rules, dfd, tupid);
	while(!list_empty(&rules)) {
		r = list_entry(rules.next, struct rule, list);
		list_del(&r->list);
		free(r);
	}
	return rc;

syntax_error:
	fprintf(stderr, "Syntax error parsing Tupfile\n  Line was: %s", line);
	return -1;
}

static int parse_rule(char *p, struct list_head *rules)
{
	char *input, *cmd, *output;
	char *ie, *ce;
	struct rule *r;

	input = p;
	while(*input == ' ')
		input++;
	p = strstr(p, ">>");
	if(!p)
		return -1;
	ie = p - 1;
	while(*ie == ' ')
		ie--;
	p += 2;
	cmd = p;
	while(*cmd == ' ')
		cmd++;
	p = strstr(p, ">>");
	if(!p)
		return -1;
	ce = p - 1;
	while(*ce == ' ')
		ce--;
	p += 2;
	output = p;
	while(*output == ' ')
		output++;
	ie[1] = 0;
	ce[1] = 0;
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
	INIT_LIST_HEAD(&r->namelist.entries);
	r->namelist.num_entries = 0;
	r->namelist.totlen = 0;
	r->namelist.extlesstotlen = 0;

	list_add(&r->list, rules);

	return 0;
}

static int execute_rules(struct list_head *rules, int dfd, tupid_t dt)
{
	struct rule *r;
	struct flist f = {0, 0, 0};
	struct stat buf;

	/* fdopendir will close the file descriptor, but we still need it, so
	 * we dup() it
	 */
	flist_foreachfd(&f, dup(dfd)) {
		if(f.filename[0] == '.')
			continue;
		if(fstatat(dirfd(f._d), f.filename, &buf, 0) == 0 &&
		   S_ISDIR(buf.st_mode))
			continue;
		list_for_each_entry(r, rules, list) {
			if(file_match(r, f.filename, dt) < 0)
				return -1;
		}
	}

	list_for_each_entry(r, rules, list) {
		if(!r->foreach && r->namelist.num_entries > 0) {
			struct name_list_entry *nle;

			if(do_rule(r, &r->namelist, dt) < 0)
				return -1;

			while(!list_empty(&r->namelist.entries)) {
				nle = list_entry(r->namelist.entries.next,
						struct name_list_entry, list);
				list_del(&nle->list);
				free(nle->path);
				free(nle);
			}
		}
	}
	return 0;
}

static int file_match(struct rule *r, const char *filename, tupid_t dt)
{
	int flags = FNM_PATHNAME | FNM_PERIOD;
	int extlesslen;
	int len;
	tupid_t in_id;

	in_id = create_name_file(dt, filename);
	if(in_id < 0)
		return -1;
	len = strlen(filename);
	extlesslen = len - 1;
	while(extlesslen > 0 && filename[extlesslen] != '.')
		extlesslen--;

	if(fnmatch(r->input_pattern, filename, flags) == 0) {
		if(r->foreach) {
			struct name_list nl;
			struct name_list_entry nle;

			nle.path = strdup(filename);
			if(!nle.path) {
				perror("strdup");
				return -1;
			}
			nle.len = len;
			nle.extlesslen = extlesslen;
			nle.tupid = in_id;
			INIT_LIST_HEAD(&nl.entries);
			list_add(&nle.list, &nl.entries);
			nl.num_entries = 1;
			nl.totlen = len;
			nl.extlesstotlen = extlesslen;
			nl.dt = dt;

			if(do_rule(r, &nl, dt) < 0)
				return -1;
			free(nle.path);
		} else {
			struct name_list_entry *nle;

			nle = malloc(sizeof *nle);
			if(!nle) {
				perror("malloc");
				return -1;
			}

			nle->path = strdup(filename);
			if(!nle->path) {
				perror("strdup");
				return -1;
			}

			nle->len = len;
			nle->extlesslen = extlesslen;
			nle->tupid = in_id;

			list_add(&nle->list, &r->namelist.entries);
			r->namelist.num_entries++;
			r->namelist.totlen += len;
			r->namelist.extlesstotlen += extlesslen;
			r->namelist.dt = dt;
		}
	}
	return 0;
}

static int do_rule(struct rule *r, struct name_list *nl, tupid_t dt)
{
	struct name_list_entry *nle;
	char *cmd;
	char *output;
	tupid_t cmd_id;
	tupid_t out_id;

	cmd = tup_printf(r->command, nl);
	if(!cmd)
		return -1;
	cmd_id = create_command_file(dt, cmd);
	free(cmd);
	if(cmd_id < 0)
		return -1;

	list_for_each_entry(nle, &nl->entries, list) {
		printf("Match[%lli]: %s, %s\n", nl->dt, r->input_pattern,
		       nle->path);
		if(tup_db_create_link(nle->tupid, cmd_id) < 0)
			return -1;
	}

	output = tup_printf(r->output_pattern, nl);
	out_id = create_name_file(dt, output);
	free(output);

	if(tup_db_create_link(cmd_id, out_id) < 0)
		return -1;
	return 0;
}

static char *tup_printf(const char *cmd, struct name_list *nl)
{
	struct name_list_entry *nle;
	char *s;
	int x;
	const char *p;
	const char *next;
	const char *spc;
	int clen = strlen(cmd);

	p = cmd;
	while((p = strchr(p, '$')) !=  NULL) {
		int paste_chars;

		clen -= 2;
		p++;
		spc = p + 1;
		while(*spc && *spc != ' ')
			spc++;
		paste_chars = (nl->num_entries - 1) * (spc - p);
		if(*p == 'f') {
			clen += nl->totlen + paste_chars;
		} else if(*p == 'F') {
			clen += nl->extlesstotlen + paste_chars;
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
		spc = p + 1;
		while(*spc && *spc != ' ')
			spc++;
		if(*spc == ' ')
			spc++;
		if(*next == 'f') {
			list_for_each_entry(nle, &nl->entries, list) {
				memcpy(&s[x], nle->path, nle->len);
				x += nle->len;
				memcpy(&s[x], p, spc - p);
				x += spc - p;
			}
		} else if(*next == 'F') {
			list_for_each_entry(nle, &nl->entries, list) {
				memcpy(&s[x], nle->path, nle->extlesslen);
				x += nle->extlesslen;
				memcpy(&s[x], p, spc - p);
				x += spc - p;
			}
		}
		p = spc;
	}
	strcpy(&s[x], p);
	if((signed)strlen(s) != clen) {
		fprintf(stderr, "Error: Calculated string length (%i) didn't match actual (%i).\n", clen, strlen(s));
		return NULL;
	}
	return s;
}
