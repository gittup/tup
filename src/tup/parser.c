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
static int execute_tupfile(struct buf *b, tupid_t tupid);
static int parse_rule(char *p, struct list_head *rules);
static int execute_rules(struct list_head *rules, tupid_t dt, struct vardb *v);
static int file_match(void *rule, struct db_node *dbn);
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
	if(fd < 0)
		return -1;

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
		while((*p == ' ' || *p == '\t' || *p == '\n') && p < e)
			p++;

		line = p;
		nl = strchr(p, '\n');
		if(!nl)
			goto syntax_error;
		*nl = 0;
		if(line[0] == '#') {
			/* Skip comments */
		} else if(strcmp(line, "else") == 0) {
			if_true = !if_true;
		} else if(strcmp(line, "endif") == 0) {
			/* TODO: Nested if */
			if_true = 1;
		} else if(!if_true) {
			/* Skip the false part of an if block */
		} else if(strncmp(line, "ifeq ", 5) == 0) {
			char *paren;
			char *comma;
			char *lval;
			char *rval;

			paren = strchr(line+5, '(');
			if(!paren)
				goto syntax_error;
			lval = paren + 1;
			comma = strchr(lval, ',');
			if(!comma)
				goto syntax_error;
			rval = comma + 1;
			if(nl[-1] != ')')
				goto syntax_error;
			*comma = 0;
			nl[-1] = 0;

			lval = eval(&vdb, lval);
			rval = eval(&vdb, rval);

			if(strcmp(lval, rval) == 0) {
				if_true = 1;
			} else {
				if_true = 0;
			}
			free(lval);
			free(rval);
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
				eq = strstr(p, ":=");
				if(eq) {
					value = eq + 2;
					append = 0;
				} else {
					eq = strchr(p, '=');
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

	vardb_dump(&vdb);
	rc = execute_rules(&rules, tupid, &vdb);
	while(!list_empty(&rules)) {
		r = list_entry(rules.next, struct rule, list);
		list_del(&r->list);
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

	list_add_tail(&r->list, rules);

	return 0;
}

static int execute_rules(struct list_head *rules, tupid_t dt, struct vardb *v)
{
	struct rule *r;

	list_for_each_entry(r, rules, list) {
		char *inp;
		char *spc;
		char *p;

		inp = eval(v, r->input_pattern);
		if(!inp)
			return -1;

		p = inp;
		while((spc = strchr(p, ' ')) != NULL) {
			*spc = 0;
			if(tup_db_select_node_dir_glob(file_match, r, dt, p)<0)
				return -1;
			p = spc + 1;
		}
		if(tup_db_select_node_dir_glob(file_match, r, dt, p) < 0)
			return -1;
		free(inp);
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

	len = strlen(dbn->name);
	extlesslen = len - 1;
	while(extlesslen > 0 && dbn->name[extlesslen] != '.')
		extlesslen--;

	if(r->foreach) {
		struct name_list nl;
		struct name_list_entry nle;

		nle.path = strdup(dbn->name);
		if(!nle.path) {
			perror("strdup");
			return -1;
		}
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

		nle->path = strdup(dbn->name);
		if(!nle->path) {
			perror("strdup");
			return -1;
		}

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

		len += dollar - s;
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
