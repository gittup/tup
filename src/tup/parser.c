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
#include <ctype.h>
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
	const char *dir;
	int dirlen;
};

struct buf {
	char *s;
	int len;
};

static int fslurp(int fd, struct buf *b);
static int execute_tupfile(struct buf *b, tupid_t tupid);
static int parse_rule(char *p, struct list_head *rules);
static int execute_rules(struct list_head *rules, tupid_t dt);
static int file_match(void *rule, struct db_node *dbn);
static char *set_path(const char *name, const char *dir, int dirlen);
static int do_rule(struct rule *r, struct name_list *nl, tupid_t dt);
static char *tup_printf(const char *cmd, struct name_list *nl);
static char *eval(struct vardb *v, const char *string);

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
	close(dfd);
	/* No Tupfile means we have nothing to do */
	if(fd < 0)
		return 0;

	if((rc = fslurp(fd, &b)) < 0) {
		goto out_close_file;
	}
	rc = execute_tupfile(&b, tupid);
	free(b.s);
out_close_file:
	close(fd);

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

static int execute_tupfile(struct buf *b, tupid_t tupid)
{
	char *p, *e;
	char *line;
	char *eval_line;
	int rc;
	struct rule *r;
	struct vardb vdb;
	int if_true = 1;
	LIST_HEAD(rules);

	if(vardb_init(&vdb) < 0)
		return -1;

	p = b->s;
	e = b->s + b->len;

	while(p < e) {
		char *nl;
		while(isspace(*p) && p < e)
			p++;

		line = p;
		nl = strchr(p, '\n');
		if(!nl)
			goto syntax_error;
		*nl = 0;
		p = nl + 1;

		if(line[0] == '#') {
			/* Skip comments */
			continue;
		}

		eval_line = eval(&vdb, line);
		if(!eval_line)
			return -1;
		printf("LINE: '%s'\n", eval_line);

		if(strcmp(eval_line, "else") == 0) {
			if_true = !if_true;
		} else if(strcmp(eval_line, "endif") == 0) {
			/* TODO: Nested if */
			if_true = 1;
		} else if(!if_true) {
			/* Skip the false part of an if block */
		} else if(strncmp(eval_line, "ifeq ", 5) == 0) {
			char *paren;
			char *comma;
			char *lval;
			char *rval;

			paren = strchr(eval_line+5, '(');
			if(!paren)
				goto syntax_error;
			lval = paren + 1;
			comma = strchr(lval, ',');
			if(!comma)
				goto syntax_error;
			rval = comma + 1;
			paren = strchr(rval, ')');
			if(!paren)
				goto syntax_error;
			*comma = 0;
			*paren = 0;

			if(strcmp(lval, rval) == 0) {
				if_true = 1;
			} else {
				if_true = 0;
			}
		} else if(eval_line[0] == ':') {
			if(parse_rule(eval_line+1, &rules) < 0)
				goto syntax_error;
		} else {
			char *eq;
			char *value;
			int append;

			/* Find the += or = sign, and point value to the start
			 * of the string after that op.
			 */
			eq = strstr(eval_line, "+=");
			if(eq) {
				value = eq + 2;
				append = 1;
			} else {
				eq = strstr(eval_line, ":=");
				if(eq) {
					value = eq + 2;
					append = 0;
				} else {
					eq = strchr(eval_line, '=');
					if(!eq)
						goto syntax_error;
					value = eq + 1;
					append = 0;
				}
			}

			/* End the lval with a 0, then space-delete the end
			 * of the variable name and the beginning of the value.
			 */
			*eq = 0;
			while(isspace(*value) && *value != 0)
				value++;
			eq--;
			while(isspace(*eq) && eq > eval_line) {
				*eq = 0;
				eq--;
			}

			if(append)
				vardb_append(&vdb, eval_line, value);
			else
				vardb_set(&vdb, eval_line, value);
		}

		free(eval_line);
	}

	vardb_dump(&vdb);
	rc = execute_rules(&rules, tupid);
	while(!list_empty(&rules)) {
		r = list_entry(rules.next, struct rule, list);
		list_del(&r->list);
		free(r->input_pattern);
		free(r->output_pattern);
		free(r->command);
		free(r);
	}
	sqlite3_close(vdb.db);
	return rc;

syntax_error:
	fprintf(stderr, "Syntax error parsing Tupfile\n  Line was: %s\n", line);
	return -1;
}

static int parse_rule(char *p, struct list_head *rules)
{
	char *input, *cmd, *output;
	char *ie, *ce;
	struct rule *r;

	input = p;
	while(isspace(*input))
		input++;
	p = strstr(p, ">>");
	if(!p)
		return -1;
	ie = p - 1;
	while(isspace(*ie))
		ie--;
	p += 2;
	cmd = p;
	while(isspace(*cmd))
		cmd++;
	p = strstr(p, ">>");
	if(!p)
		return -1;
	ce = p - 1;
	while(isspace(*ce))
		ce--;
	p += 2;
	output = p;
	while(isspace(*output))
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
		r->input_pattern = strdup(input + 8);
		r->foreach = 1;
	} else {
		r->input_pattern = strdup(input);
		r->foreach = 0;
	}
	if(!r->input_pattern) {
		perror("strdup");
		return -1;
	}
	r->output_pattern = strdup(output);
	if(!r->output_pattern) {
		perror("strdup");
		return -1;
	}
	r->command = strdup(cmd);
	if(!r->command) {
		perror("strdup");
		return -1;
	}
	INIT_LIST_HEAD(&r->namelist.entries);
	r->namelist.num_entries = 0;
	r->namelist.totlen = 0;
	r->namelist.extlesstotlen = 0;

	list_add_tail(&r->list, rules);

	return 0;
}

static int execute_rules(struct list_head *rules, tupid_t dt)
{
	struct rule *r;

	list_for_each_entry(r, rules, list) {
		char *spc;
		char *p;
		const char *file;
		tupid_t subdir;

		p = r->input_pattern;
		do {
			spc = strchr(p, ' ');
			if(spc)
				*spc = 0;

			subdir = find_dir_tupid_dt(dt, p, &file);
			if(subdir < 0)
				return -1;
			if(subdir != dt)
				if(tup_db_create_link(subdir, dt) < 0)
					return -1;
			if(p != file) {
				/* Note that dirlen should be file-p-1, but we
				 * add 1 to account for the trailing '/' that
				 * will be added.
				 */
				p[file-p-1] = 0;
				r->dir = p;
				r->dirlen = file-p;
			} else {
				r->dir = "";
				r->dirlen = 0;
			}
			if(tup_db_select_node_dir_glob(file_match, r,
						       subdir, file) < 0)
				return -1;

			if(spc)
				p = spc + 1;
		} while(spc != NULL);
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

static int file_match(void *rule, struct db_node *dbn)
{
	struct rule *r = rule;
	int extlesslen;
	int len;

	len = strlen(dbn->name) + r->dirlen;
	extlesslen = len - 1;
	while(extlesslen > 0 && dbn->name[extlesslen] != '.')
		extlesslen--;

	if(r->foreach) {
		struct name_list nl;
		struct name_list_entry nle;

		nle.path = set_path(dbn->name, r->dir, r->dirlen);
		if(!nle.path)
			return -1;

		nle.len = len;
		nle.extlesslen = extlesslen;
		nle.tupid = dbn->tupid;
		INIT_LIST_HEAD(&nl.entries);
		list_add(&nle.list, &nl.entries);
		nl.num_entries = 1;
		nl.totlen = len;
		nl.extlesstotlen = extlesslen;
		nl.dt = dbn->dt;

		if(do_rule(r, &nl, dbn->dt) < 0)
			return -1;
		free(nle.path);
	} else {
		struct name_list_entry *nle;

		nle = malloc(sizeof *nle);
		if(!nle) {
			perror("malloc");
			return -1;
		}

		nle->path = set_path(dbn->name, r->dir, r->dirlen);
		if(!nle->path)
			return -1;

		nle->len = len;
		nle->extlesslen = extlesslen;
		nle->tupid = dbn->tupid;

		list_add(&nle->list, &r->namelist.entries);
		r->namelist.num_entries++;
		r->namelist.totlen += len;
		r->namelist.extlesstotlen += extlesslen;
		r->namelist.dt = dbn->dt;
	}
	return 0;
}

static char *set_path(const char *name, const char *dir, int dirlen)
{
	char *path;

	if(dirlen) {
		int nlen;

		nlen = strlen(name);
		/* +1 for '/', +1 for nul */
		path = malloc(nlen + dirlen + 1);
		if(!path) {
			perror("malloc");
			return NULL;
		}

		memcpy(path, dir, dirlen-1);
		path[dirlen-1] = '/';
		strcpy(path + dirlen, name);
	} else {
		path = strdup(name);
		if(!path) {
			perror("strdup");
			return NULL;
		}
	}
	return path;
}

static int do_rule(struct rule *r, struct name_list *nl, tupid_t dt)
{
	struct name_list_entry *nle;
	char *cmd;
	char *output;
	int node_created = 0;
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
	out_id = tup_db_create_node_part(dt, output, -1, TUP_NODE_FILE,
					 TUP_FLAGS_MODIFY, &node_created);
	if(out_id < 0)
		return -1;
	if(node_created)
		if(tup_db_set_dependent_dir_flags(dt, TUP_FLAGS_CREATE) < 0)
			return -1;
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
	while((p = strchr(p, '%')) !=  NULL) {
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
	while((next = strchr(p, '%')) !=  NULL) {
		memcpy(&s[x], p, next-p);
		x += next-p;

		next++;
		p = next + 1;
		spc = p;
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

static char *eval(struct vardb *v, const char *string)
{
	int len = 0;
	const char *dollar;
	char *ret;
	char *p;
	const char *s;

	s = string;
	while((dollar = strchr(s, '$')) != NULL) {
		const char *rparen;
		const char *var;
		int vlen;

		len += dollar - s;
		if(dollar[1] != '(')
			goto syntax_error;
		rparen = strchr(dollar+1, ')');
		if(!rparen)
			goto syntax_error;

		var = dollar + 2;
		vlen = vardb_len(v, var, rparen-var);
		len += vlen;
		s = rparen + 1;
	}
	len += strlen(s);

	ret = malloc(len+1);
	if(!ret) {
		perror("malloc");
		return NULL;
	}

	p = ret;
	s = string;
	while((dollar = strchr(s, '$')) != NULL) {
		const char *rparen;
		const char *var;

		memcpy(p, s, dollar-s);
		p += dollar-s;

		if(dollar[1] != '(')
			goto syntax_error;
		rparen = strchr(dollar+1, ')');
		if(!rparen)
			goto syntax_error;

		var = dollar + 2;
		if(vardb_get(v, var, rparen-var, &p) < 0)
			return NULL;

		s = rparen + 1;
	}
	strcpy(p, s);

	if((signed)strlen(ret) != len) {
		fprintf(stderr, "Length mismatch: expected %i bytes, wrote %i\n", len, strlen(ret));
	}

	return ret;

syntax_error:
	fprintf(stderr, "Syntax error: expected $(\n");
	return NULL;
}
