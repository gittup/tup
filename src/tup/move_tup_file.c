#include "fileio.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int move_tup_file(const tupid_t tupid, const char *tupsrc, const char *tupdst)
{
	char oldfilename[] = ".tup/XXXXXX/" SHA1_X;
	char newfilename[] = ".tup/XXXXXX/" SHA1_X;

	memcpy(oldfilename + 5, tupsrc, 6);
	memcpy(newfilename + 5, tupdst, 6);
	memcpy(newfilename + 12, tupid, sizeof(tupid_t));
	memcpy(oldfilename + 12, tupid, sizeof(tupid_t));
	DEBUGP("move tup file '%s/%.*s' to '%s/%.*s'\n",
	       tupsrc, 8, tupid, tupdst, 8, tupid);

	if(rename(oldfilename, newfilename) < 0) {
		fprintf(stderr, "Error renaming '%s' to '%s': %s\n",
			oldfilename, newfilename, strerror(errno));
		return -1;
	}
	return 0;
}
