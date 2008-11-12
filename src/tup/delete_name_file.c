#include "fileio.h"
#include "db.h"

int delete_name_file(tupid_t tupid)
{
	if(tup_db_exec("delete from node where id=%lli", tupid) != 0)
		return -1;

	/* TODO: Do we want to delete all links? I assume so */
	if(tup_db_exec("delete from link where from_id=%lli or to_id=%lli", tupid, tupid) != 0)
		return -1;
	return 0;
}
