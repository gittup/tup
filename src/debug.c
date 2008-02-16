#include "debug.h"

static int debugging = 0;
static const char *dstring = NULL;

int debug_enabled(void)
{
	return debugging;
}

const char *debug_string(void)
{
	return dstring;
}

void debug_enable(const char *label)
{
	debugging = 1;
	dstring = label;
}

void debug_disable(void)
{
	debugging = 0;
}
