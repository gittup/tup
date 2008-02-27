#include <stdio.h>
#include <string.h>
#include "flist.h"
#include "graph.h"
#include "tupid.h"
#include "debug.h"

#define GRAPH_NAME "/home/marf/test%03i.dot"

static int build_graph(void);
static int add_file(const tupid_t tupid, struct node *src, struct list_head *p);
static int process_file(struct node *n, struct list_head *p);
static int execute_graph(struct node *root);

int main(void)
{
	int rc;
	debug_enable("tup.updater");
	rc = build_graph();
	if(rc < 0)
		return 1;
	return 0;
}

static int build_graph(void)
{
	LIST_HEAD(plist);
	struct flist f;
	unsigned int x;
	struct node *root;
	struct node *cur;
	char add_pathnames[][13] = {
		".tup/attrib/",
		".tup/modify/",
	};

	root = create_node(TUPDIR_HASH);
	if(!root)
		return -1;

	/* First attach all nodes in the relevant .tup directories to the
	 * root.
	 */
	cur = root;
	for(x=0; x<sizeof(add_pathnames) / sizeof(add_pathnames[0]); x++) {
		flist_foreach(&f, add_pathnames[x]) {
			if(f.filename[0] == '.')
				continue;
			if(add_file(f.filename, root, &plist) < 0)
				return -1;
		}
	}

	while(!list_empty(&plist)) {
		cur = list_entry(plist.next, struct node, processing);
		DEBUGP("Processing node: %.*s\n", sizeof(tupid_t), cur->tupid);
		if(process_file(cur, &plist) < 0)
			return -1;
		list_del(&cur->processing);
		INIT_LIST_HEAD(&cur->processing);
	}

	dump_graph(GRAPH_NAME);
	execute_graph(root);
	return 0;
}

static int add_file(const tupid_t tupid, struct node *src, struct list_head *p)
{
	struct node *n;

	if((n = find_node(tupid)) != NULL) {
		goto edge_create;
	}
	n = create_node(tupid);
	if(!n)
		return -1;

	DEBUGP("Created node: %.*s\n", sizeof(tupid_t), tupid);
	if(list_empty(&n->processing))
		list_add_tail(&n->processing, p);

edge_create:
	if(create_edge(src, n) < 0)
		return -1;
	return 0;
}

static int process_file(struct node *n, struct list_head *p)
{
	int rc = 0;
	struct flist f;
	char object_dir[] = ".tup/object/" SHA1_X;

	memcpy(object_dir + 12, n->tupid, sizeof(tupid_t));
	flist_foreach(&f, object_dir) {
		if(f.filename[0] == '.')
			continue;
		if(strcmp(f.filename, "cmd") == 0 ||
		   strcmp(f.filename, "name") == 0)
			continue;
		if((rc = add_file(f.filename, n, p)) < 0)
			break;
	};

	return rc;
}

static int execute_graph(struct node *root)
{
	LIST_HEAD(plist);

	DEBUGP("Root is: %.*s\n", 8, root->tupid);
	list_add(&root->processing, &plist);

	while(!list_empty(&plist)) {
		struct node *n;
		n = list_entry(plist.next, struct node, processing);
		DEBUGP("N is: %.*s\n", 8, n->tupid);
		if(n->incoming_count) {
			list_del(&n->processing);
			INIT_LIST_HEAD(&n->processing);
			continue;
		}
		DEBUGP("Run command for %.*s\n", 8, n->tupid);
		while(!list_empty(&n->edges)) {
			struct edge *e;
			e = list_entry(n->edges.next, struct edge, list);
			if(list_empty(&e->dest->processing)) {
				list_add_tail(&e->dest->processing, &plist);
			}
			remove_edge(e);
		}
		remove_node(n);
		dump_graph(GRAPH_NAME);
	}
	return 0;
}
