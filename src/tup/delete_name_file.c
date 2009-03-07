#include "fileio.h"
#include "db.h"

int delete_name_file(tupid_t tupid)
{
	if(tup_db_unflag_create(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(tup_db_unflag_delete(tupid) < 0)
		return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid) < 0)
		return -1;
	return 0;
}

int delete_dir_file(tupid_t tupid)
{
	/* Deleted directories have to recurse to delete sub-nodes. */
	if(tup_db_delete_dir(tupid) < 0)
		return -1;
	if(delete_name_file(tupid) < 0)
		return -1;
	return 0;
}
