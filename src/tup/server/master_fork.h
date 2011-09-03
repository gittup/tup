#ifndef tup_master_fork_h
#define tup_master_fork_h

#include "tup/compat.h"

struct execmsg {
	int sid;
	int dirlen;
	int cmdlen;
	char text[PATH_MAX * 2];
};

int master_fork_exec(struct execmsg *em, int size, int *status);

#endif
