#include "fileio.h"
#include "db.h"

int delete_name_file(tupid_t tupid, tupid_t dt, tupid_t sym)
{
	if(tup_db_unflag_create(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(tup_db_unflag_delete(tupid) < 0)
		return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid, dt, sym) < 0)
		return -1;
	return 0;
}
