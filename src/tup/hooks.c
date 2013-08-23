/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hooks.h"
#include "config.h"
#include "option.h"

int run_pre_init_hooks(void)
{
	return 0;
}

/* "private" export from tup/main.c */
int variant_command(int argc, char **argv);
static int post_init_variant_hook(void)
{
	char argv0[] = "post_init_variants";
	char *argv[3];
	const char* variants;
	char *variants_copy;
	char* v;

	argv[0] = argv0;
	argv[2] = NULL;

	variants = tup_option_get_string("post_init.variants");
	if (variants == NULL)
		return 0;

	variants_copy = strdup(variants);
	if (variants_copy == NULL) {
		fprintf(stderr, "tup error: Failed to allocate memory for variants string\n");
		return -1;
	}

	/* The create_variant command expects find_tup_dir to have
	   run already, but as a post-init hook we're too early, run it now */
	if (find_tup_dir() != 0) {
		fprintf(stderr, "tup internal error: Failed to find .tup dir in post-init hook\n");
		free(variants_copy);
		return -1;
	}

	v = strtok(variants_copy, " ,");
	while (v != NULL) {
		argv[1] = v;
		if (variant_command(2, argv) != 0) {
			fprintf(stderr, "tup error: Post-init hook failed creating variant\n");
			fprintf(stderr, "tup error:   Failed variant was: %s\n", v);
			break;
		}
		v = strtok(NULL, " ,");
	}

	free(variants_copy);

	if (v == NULL)
		return 0;
	else
		return -1;
}

int run_post_init_hooks(void)
{
	printf("Running post-init hooks...\n");

	if (post_init_variant_hook() != 0)
		return -1;

	return 0;
}
