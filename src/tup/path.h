#include "tupid.h"

int watch_path(tupid_t dt, int dfd, const char *file, int tmp_list,
	       int (*callback)(tupid_t newdt, int dfd, const char *file));
