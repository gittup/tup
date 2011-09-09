#ifndef tup_master_fork_h
#define tup_master_fork_h

#include "tup/compat.h"

struct execmsg {
	int sid;
	int dirlen;
	int cmdlen;
};

int master_fork_exec(struct execmsg *em, const char *dir, const char *cmd,
		     int *status);

#endif
