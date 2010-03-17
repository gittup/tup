#include "tup/colors.h"
#include "tup/db.h"

const char *color_type(int type)
{
	const char *color = "";

	switch(type) {
		case TUP_NODE_DIR:
			color = "[33";
			break;
		case TUP_NODE_CMD:
			color = "[34";
			break;
		case TUP_NODE_GENERATED:
			color = "[35";
			break;
		case TUP_NODE_FILE:
			/* If a generated node becomes a normal file (t6031) */
			color = "[37";
			break;
	}
	return color;
}

const char *color_append_normal(void)
{
	return "m";
}

const char *color_append_reverse(void)
{
	return ";07m";
}

const char *color_reverse(void)
{
	return "[07m";
}

const char *color_end(void)
{
	return "[0m";
}

const char *color_final(void)
{
	return "[07;32m";
}
