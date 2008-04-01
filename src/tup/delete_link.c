#include "fileio.h"
#include "tupid.h"
#include <unistd.h>

int delete_link(const tupid_t a, const tupid_t b)
{
	char linkfile[] = ".tup/object/" SHA1_XD "/" SHA1_X;

	tupid_to_xd(linkfile+12, a);
	memcpy(linkfile+14+sizeof(tupid_t), b, sizeof(tupid_t));
	return unlink(linkfile);
}
