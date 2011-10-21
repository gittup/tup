#include "tup/colors.h"
#include "tup/db.h"

static int disabled = 0;

void color_disable(void)
{
	disabled = 1;
}

const char *color_type(int type)
{
	const char *color = "";

	if(disabled)
		return "";

	switch(type) {
		case TUP_NODE_ROOT:
			/* Overloaded to mean a node in error (can be re-used
			 * since TUP_NODE_ROOT is never displayed).
			 */
			color = "[31";
			break;
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
	if(disabled)
		return "";
	return "m";
}

const char *color_append_reverse(void)
{
	if(disabled)
		return "";
	return ";07m";
}

const char *color_reverse(void)
{
	if(disabled)
		return "";
	return "[07m";
}

const char *color_end(void)
{
	if(disabled)
		return "";
	return "[0m";
}

const char *color_final(void)
{
	if(disabled)
		return "";
	return "[07;32m";
}
