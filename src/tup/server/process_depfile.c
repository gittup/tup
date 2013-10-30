#include "process_depfile.h"
#include "tup/access_event.h"
#include "tup/file.h"
#include "tup/server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static struct mapping *find_mapping(struct file_info *finfo, const char *path)
{
	struct mapping *map;

	LIST_FOREACH(map, &finfo->mapping_list, list) {
		if(strcmp(path, map->realname) == 0) {
			return map;
		}
	}
	return NULL;
}

int process_depfile(struct server *s, int fd)
{
	char event1[PATH_MAX];
	char event2[PATH_MAX];
	FILE *f;

	f = fdopen(fd, "rb");
	if(!f) {
		perror("fdopen");
		fprintf(stderr, "tup error: Unable to open dependency file for post-processing.\n");
		return -1;
	}
	while(1) {
		struct access_event event;

		if(fread(&event, sizeof(event), 1, f) != 1) {
			if(!feof(f)) {
				perror("fread");
				fprintf(stderr, "tup error: Unable to read the access_event structure from the dependency file.\n");
				return -1;
			}
			break;
		}

		if(!event.len)
			continue;

		if(event.len >= PATH_MAX - 1) {
			fprintf(stderr, "tup error: Size of %i bytes is longer than the max filesize\n", event.len);
			return -1;
		}
		if(event.len2 >= PATH_MAX - 1) {
			fprintf(stderr, "tup error: Size of %i bytes is longer than the max filesize\n", event.len2);
			return -1;
		}

		if(fread(&event1, event.len + 1, 1, f) != 1) {
			perror("fread");
			fprintf(stderr, "tup error: Unable to read the first event from the dependency file.\n");
			return -1;
		}
		if(fread(&event2, event.len2 + 1, 1, f) != 1) {
			perror("fread");
			fprintf(stderr, "tup error: Unable to read the second event from the dependency file.\n");
			return -1;
		}

		if(event1[event.len] != '\0' || event2[event.len2] != '\0') {
			fprintf(stderr, "tup error: Missing null terminator in access_event\n");
			return -1;
		}

		if(event.at == ACCESS_WRITE) {
			struct mapping *map;

			map = malloc(sizeof *map);
			if(!map) {
				perror("malloc");
				return -1;
			}
			map->realname = strdup(event1);
			if(!map->realname) {
				perror("strdup");
				return -1;
			}
			map->tmpname = strdup(event1);
			if(!map->tmpname) {
				perror("strdup");
				return -1;
			}
			map->tent = NULL; /* This is used when saving deps */
			LIST_INSERT_HEAD(&s->finfo.mapping_list, map, list);
		} else if(event.at == ACCESS_RENAME) {
			struct mapping *map;
			map = find_mapping(&s->finfo, event2);
			if(map) {
//				unlink(map->tmpname);
				del_map(map);
			}

			map = find_mapping(&s->finfo, event1);
			if(!map) {
				return -1;
			}

			free(map->realname);
			map->realname = strdup(event2);
			if(!map->realname) {
				perror("strdup");
				return -1;
			}
			free(map->tmpname);
			map->tmpname = strdup(event2);
			if(!map->tmpname) {
				perror("strdup");
				return -1;
			}
		} else if(event.at == ACCESS_UNLINK) {
			struct mapping *map;
			map = find_mapping(&s->finfo, event1);
			if(map) {
//				unlink(map->tmpname);
				del_map(map);
			}
		}
		if(handle_file(event.at, event1, event2, &s->finfo) < 0) {
			fprintf(stderr, "tup error: Failed to call handle_file on event '%s'\n", event1);
			return -1;
		}
	}

	/* Since this file is FILE_FLAG_DELETE_ON_CLOSE, the temporary file
	 * goes away after this fclose.
	 */
	if(fclose(f) < 0) {
		perror("fclose");
		return -1;
	}
	return 0;
}
