#define _GNU_SOURCE /* TODO: For asprintf */
#include "graph.h"
#include "debug.h"
#include "db.h"
#include "array_size.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int db_add(struct graph *g, struct node *n);
static int db_remove(struct graph *g, struct node *n);
static void dump_node(FILE *f, struct node *n);

struct node *find_node(const struct graph *g, tupid_t tupid)
{
	int dbrc;
	struct node *n;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "select ptr from node_map where id=?";
	int res;

	if(!stmt) {
		if(sqlite3_prepare_v2(g->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(g->db), s);
			return NULL;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(g->db));
		return NULL;
	}

	dbrc = sqlite3_step(stmt);
	if(dbrc == SQLITE_DONE) {
		n = NULL;
		goto out_reset;
	}
	if(dbrc != SQLITE_ROW) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(g->db));
		n = NULL;
		goto out_reset;
	}

	res = sqlite3_column_int(stmt, 0);
	n = (struct node*)res;

out_reset:
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(g->db));
		return NULL;
	}

	return n;
}

struct node *create_node(struct graph *g, tupid_t tupid, const char *name,
			 int type, int flags)
{
	struct node *n;

	n = malloc(sizeof *n);
	if(!n) {
		perror("malloc");
		return NULL;
	}
	n->edges = NULL;
	n->tupid = tupid;
	n->incoming_count = 0;
	n->name = strdup(name);
	if(!n->name) {
		perror("strdup");
		return NULL;
	}
	n->state = STATE_INITIALIZED;
	n->type = type;
	n->flags = flags;
	list_add(&n->list, &g->plist);

	if(n->type == TUP_NODE_CMD ||
	   (n->type == TUP_NODE_FILE && n->flags == TUP_FLAGS_DELETE)) {
		g->num_nodes++;
	}

	if(db_add(g, n) < 0)
		return NULL;
	return n;
}

void remove_node(struct graph *g, struct node *n)
{
	list_del(&n->list);
	if(n->edges) {
		DEBUGP("Warning: Node %lli still has edges.\n", n->tupid);
	}
	db_remove(g, n);
	/* TODO: block pool */
	free(n);
}

int create_edge(struct node *n1, struct node *n2)
{
	struct edge *e;

	/* TODO: block pool */
	e = malloc(sizeof *e);
	if(!e) {
		perror("malloc");
		return -1;
	}

	e->dest = n2;

	/* TODO: slist add? */
	e->next = n1->edges;
	n1->edges = e;

	n2->incoming_count++;
	return 0;
}

struct edge *remove_edge(struct edge *e)
{
	struct edge *tmp;
	tmp = e->next;
	e->dest->incoming_count--;
	/* TODO: block pool */
	free(e);
	return tmp;
}

int create_graph(struct graph *g)
{
	int x;
	int rc;
	const char *sql[] = {
		"create table node_map (id integer primary key not null, ptr integer not null)",
	};

	if(sizeof(struct node *) != 4) {
		fprintf(stderr, "Error: sizeof node pointer is not 32 bits (size = %i bytes). This needs to be fixed.\n", sizeof(struct node *));
		return -1;
	}

	INIT_LIST_HEAD(&g->node_list);
	INIT_LIST_HEAD(&g->plist);

	rc = sqlite3_open(":memory:", &g->db);
	if(rc != 0) {
		fprintf(stderr, "Unable to create in-memory database: %s\n",
			sqlite3_errmsg(g->db));
		return -1;
	}
	for(x=0; x<ARRAY_SIZE(sql); x++) {
		char *errmsg;
		if(sqlite3_exec(g->db, sql[x], NULL, NULL, &errmsg) != 0) {
			fprintf(stderr, "SQL error: %s\nQuery was: %s",
				errmsg, sql[x]);
			return -1;
		}
	}

	g->root = create_node(g, 0, "root", TUP_NODE_ROOT, TUP_FLAGS_NONE);
	if(!g->root)
		return -1;
	list_move(&g->root->list, &g->node_list);
	g->num_nodes = 0;
	return 0;
}

void dump_graph(const struct graph *g, const char *filename)
{
	static int count = 0;
	struct node *n;
	char *realfile;
	FILE *f;

	if(asprintf(&realfile, filename, getpid(), count) < 0) {
		perror("asprintf");
		return;
	}
	fprintf(stderr, "Dumping graph '%s'\n", realfile);
	count++;
	f = fopen(realfile, "w");
	if(!f) {
		perror(realfile);
		return;
	}
	fprintf(f, "digraph G {\n");
	list_for_each_entry(n, &g->node_list, list) {
		dump_node(f, n);
	}
	list_for_each_entry(n, &g->plist, list) {
		dump_node(f, n);
	}
	fprintf(f, "}\n");
	fclose(f);
}

static int db_add(struct graph *g, struct node *n)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "insert into node_map(id, ptr) values(?, ?)";

	if(!stmt) {
		if(sqlite3_prepare_v2(g->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(g->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, n->tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}
	if(sqlite3_bind_int(stmt, 2, (int)n) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}

	return 0;
}

static int db_remove(struct graph *g, struct node *n)
{
	int rc;
	static sqlite3_stmt *stmt = NULL;
	static char s[] = "delete from node_map where id=?";

	if(!stmt) {
		if(sqlite3_prepare_v2(g->db, s, sizeof(s), &stmt, NULL) != 0) {
			fprintf(stderr, "SQL Error: %s\nStatement was: %s\n",
				sqlite3_errmsg(g->db), s);
			return -1;
		}
	}

	if(sqlite3_bind_int64(stmt, 1, n->tupid) != 0) {
		fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if(sqlite3_reset(stmt) != 0) {
		fprintf(stderr, "SQL reset error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}

	if(rc != SQLITE_DONE) {
		fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(g->db));
		return -1;
	}

	return 0;
}

static void dump_node(FILE *f, struct node *n)
{
	struct edge *e;
	int color = 0;
	if(n->flags & TUP_FLAGS_CREATE)
		color |= 0x00bb00;
	if(n->flags & TUP_FLAGS_DELETE)
		color |= 0xff0000;
	if(n->flags & TUP_FLAGS_MODIFY)
		color |= 0x0000ff;
	fprintf(f, "tup%lli [label=\"%s (%i)\",color=\"#%06x\"];\n",
		n->tupid, n->name, n->incoming_count, color);
	/* TODO: slist_for_each? */
	for(e=n->edges; e; e=e->next) {
		fprintf(f, "tup%lli -> tup%lli [dir=back];\n",
			e->dest->tupid, n->tupid);
	}
}
