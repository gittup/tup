#include "fileio.h"
#include "config.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>

int canonicalize(const char *path, char *out, int len)
{
	if(path[0] == '/')
		return canonicalize2(path, "", out, len);
	else
		return canonicalize2("", path, out, len);
}

int canonicalize2(const char *path, const char *file, char *out, int len)
{
	int sz;

	if(path[0] == '/') {
		int ttl = get_tup_top_len();

		/* Check if the path matches, and since get_tup_top() won't
		 * have a trailing slash, make sure the path has that too.
		 */
		if(strncmp(path, get_tup_top(), ttl) != 0 || path[ttl] != '/') {
			DEBUGP("Cannot canonicalize path '%s' since it is not "
			       "in the .tup hierarchy.\n", path);
			return -1;
		}
		if(file[0]) {
			sz = snprintf(out, len, "%s/%s", path + ttl + 1, file);
		} else {
			sz = snprintf(out, len, "%s", path + ttl + 1);
		}
	} else {
		/* If it's a relative path, prepend the subdirectory that the
		 * user invoked the command in relative to where .tup/ exists.
		 */
		const char *dir = get_sub_dir();
		if(dir[0]) {
			sz = snprintf(out, len, "%s/%s/%s", dir, path, file);
		} else {
			if(path[0])
				sz = snprintf(out, len, "%s/%s", path, file);
			else
				sz = snprintf(out, len, "%s", file);
		}
	}
	if(sz >= len) {
		fprintf(stderr, "No room for file '%s', '%s'\n", path, file);
		return -1;
	}

	sz = canonicalize_string(out, sz);
	if(strncmp(out, "../", 3) == 0) {
		DEBUGP("Relative path '%s' is outside the tup hierarchy.\n",
		       out);
		return -1;
	}
	return sz;
}

int canonicalize_string(char *str, int sz)
{
	int x;

	if(sz == 0)
		return 0;

	for(x=0; x<sz; x++) {
		if(x) {
			if(str[x] == '/' && str[x-1] == '/') {
				memmove(str+x, str+x+1, sz-x);
				sz--;
				x--;
			}
		}
	}
	for(x=0; x<sz; x++) {
		if(x >= 1) {
			if(str[x-0] == '/' &&
			   str[x-1] == '.' &&
			   (x == 1 || str[x-2] == '/')) {
				memmove(str+x-1, str+x+1, sz-x);
				sz -= 2;
				x--;
			}
		}
		if(x >= 3) {
			int sl;
			if(str[x-0] == '/' &&
			   str[x-1] == '.' &&
			   str[x-2] == '.' &&
			   str[x-3] == '/') {
				for(sl = x - 4; sl >= 0; sl--) {
					if(str[sl] == '/') {
						memmove(str+sl, str+x, sz-x+1);
						sz -= x-sl;
						x = sl;
						goto done;
					}
				}
				memmove(str, str+x+1, sz-x);
				sz -= (x + 1);
				x = 0;
done:
				;
			}
		}
	}
	while(sz > 0 && str[sz-1] == '/') {
		str[sz-1] = 0;
		sz--;
	}
	if(sz == 0) {
		sz = 1;
		str[0] = '.';
		str[1] = 0;
	}
	return sz;
}
