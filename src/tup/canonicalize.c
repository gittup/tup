#include "fileio.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

int canonicalize(const char *path, const char *file, char *out, int len)
{
	int sz;
	int x;

	if(path[0] == '/') {
		int ttl = get_tup_top_len();

		/* Check if the path matches, and since get_tup_top() won't
		 * have a trailing slash, make sure the path has that too.
		 */
		if(strncmp(path, get_tup_top(), ttl) != 0 || path[ttl] != '/') {
			fprintf(stderr, "Error: Cannot canonicalize path '%s' "
				"since it is not in the .tup hierarchy.\n",
				path);
			return -1;
		}
		sz = snprintf(out, len, "%s/%s", path + ttl + 1, file);
		if(sz >= len) {
			fprintf(stderr, "Out of room for file '%s/%s'\n",
				path, file);
			return -1;
		}
	} else {
		const char *dir = get_sub_dir();
		if(dir[0]) {
			sz = snprintf(out, len, "%s/%s/%s", dir, path, file);
		} else {
			if(path[0])
				sz = snprintf(out, len, "%s/%s", path, file);
			else
				sz = snprintf(out, len, "%s", file);
		}
		if(sz >= len) {
			fprintf(stderr, "Out of room for file '%s/%s/%s'\n",
				get_sub_dir(), path, file);
			return -1;
		}
	}

	for(x=0; x<sz; x++) {
		if(x) {
			if(out[x] == '/' && out[x-1] == '/') {
				memmove(out+x, out+x+1, sz-x);
				sz--;
				x--;
			}
		}
	}
	for(x=0; x<sz; x++) {
		if(x >= 1) {
			if(out[x-0] == '/' &&
			   out[x-1] == '.' &&
			   (x == 1 || out[x-2] == '/')) {
				memmove(out+x-1, out+x+1, sz-x);
				sz -= 2;
				x--;
			}
		}
		if(x >= 3) {
			int slash;
			if(out[x-0] == '/' &&
			   out[x-1] == '.' &&
			   out[x-2] == '.' &&
			   out[x-3] == '/') {
				for(slash = x - 4; slash >= 0; slash--) {
					if(out[slash] == '/') {
						memmove(out+slash, out+x, sz-x+1);
						sz -= x-slash;
						x = slash;
						goto done;
					}
				}
				memmove(out, out+x+1, sz-x);
				sz -= (x + 1);
				x = 0;
done:
				;
			}
		}
	}
	if(strncmp(out, "../", 3) == 0) {
		fprintf(stderr, "Error: Relative path '%s' is outside the tup "
			"hierarchy.\n", out);
		return -1;
	}
	return 0;
}
