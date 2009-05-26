#include "tupid.h"

int watch_path(tupid_t dt, int dfd, const char *file, int tmpdb,
	       int (*callback)(tupid_t newdt, int dfd, const char *file));
