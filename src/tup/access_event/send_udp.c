#include "tup/access_event.h"
#include <stdio.h>

void tup_send_event(const char *file, int len, const char *file2, int len2, int at)
{
	if(file || len || file2 || len2 || at) {}
	fprintf(stderr, "tup_send_event: Operation unsupported on this platform.\n");
}
