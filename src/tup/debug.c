#include "debug.h"
#include <stdio.h>

static int debugging = 0;
static int server_debug = 0;
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

void server_enable_debug(void)
{
	server_debug = 1;
}

int server_debug_enabled(void)
{
	return server_debug;
}
