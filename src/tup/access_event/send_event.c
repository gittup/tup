#include "tup/access_event.h"
#include <stdio.h>
#include <fcntl.h>

void tup_send_event(const char *file, int len, const char *file2, int len2, int at)
{
	char path[PATH_MAX];

	if(file2 || len2 || at) {/*TODO */}
	if(snprintf(path, sizeof(path), TUP_VAR_VIRTUAL_DIR "/%.*s", len, file) >= (signed)sizeof(path)) {
		fprintf(stderr, "tup internal error: path is too small in tup_send_event()\n");
		return;
	}

	open(path, O_RDONLY);
}
