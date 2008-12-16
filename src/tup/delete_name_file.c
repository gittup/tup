#include "fileio.h"
#include "db.h"

int delete_name_file(tupid_t tupid)
{
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid) < 0)
		return -1;
	return 0;
}
