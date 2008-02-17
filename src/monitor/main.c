#if 0
        Step 1.  Start at initial directory foo.  Add watch.
        
        Step 2.  Setup handlers for watch created in Step 1.
                 Specifically, ensure that a directory created
                 in foo will result in a handled CREATE_SUBDIR
                 event.
        
        Step 3.  Read the contents of foo.
        
        Step 4.  For each subdirectory of foo read in step 3, repeat
                 step 1.
        
        Step 5.  For any CREATE_SUBDIR event on bar, if a watch is
                 not yet created on bar, repeat step 1 on bar.
#endif

/* _GNU_SOURCE for asprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <unistd.h>
#include "dircache.h"
#include "flist.h"
#include "debug.h"

static int isdir(const char *file);
static int watch_path(int fd, const char *path, const char *file);
static int events_same(struct inotify_event *a, struct inotify_event *b);

int main(int argc, char **argv)
{
	int fd;
	int x;
	int rc = 0;
	struct timeval t1, t2;
	const char *path = NULL;
	static char buf[(sizeof(struct inotify_event) + 16) * 1024];
	static char last[sizeof(struct inotify_event) + MAXPATHLEN];

	for(x=1; x<argc; x++) {
		if(strcmp(argv[x], "-d") == 0) {
			debug_enable("monitor");
		} else {
			path = argv[x];
		}
	}
	if(!path) {
		fprintf(stderr, "Usage: %s [-d] path_to_watch\n", argv[0]);
		return 1;
	}

	gettimeofday(&t1, NULL);
	fd = inotify_init();
	if(fd < 0) {
		perror("inotify_init");
		return -1;
	}

	if(watch_path(fd, path, "") < 0) {
		rc = -1;
		goto close_inot;
	}

	gettimeofday(&t2, NULL);
	fprintf(stderr, "Initialized in %f seconds.\n",
		(double)(t2.tv_sec - t1.tv_sec) +
		(double)(t2.tv_usec - t1.tv_usec)/1e6);

	while((x = read(fd, buf, sizeof(buf))) > 0) {
		int offset = 0;

		while(offset < x) {
			struct inotify_event *e = (void*)((char*)buf + offset);

			/* Remove duplicate events (ie: for a program writing
			 * to a file, the IN_MODIFY event will be received
			 * multiple times).
			 */
			if(events_same((struct inotify_event*)last, e) == 0)
				goto next_event;

			if(sizeof(*e) + e->len < sizeof(last)) {
				memcpy(last, e, sizeof(*e) + e->len);
			} else {
				fprintf(stderr, "Error: last event size is "
					"to small.\n");
				return -1;
			}

			if(e->len > 0) {
				printf("%08x:%s%s\n", e->mask,
				       dircache_lookup(e->wd), e->name);
			} else {
				printf("%08x:%s\n", e->mask,
				       dircache_lookup(e->wd));
			}
			if(e->mask & IN_ISDIR) {
				if(e->mask & IN_CREATE)
					watch_path(fd, dircache_lookup(e->wd),
						   e->name);
			}
			if(e->mask & IN_IGNORED) {
				dircache_del(e->wd);
			}
next_event:
			offset += sizeof(*e) + e->len;
		}
	}

close_inot:
	close(fd);
	return 0;
}

static int isdir(const char *file)
{
	struct stat buf;
	if(stat(file, &buf) == 0) {
		if(buf.st_mode & S_IFDIR)
			return 1;
		return 0;
	} else {
		return 0;
	}
}

static int watch_path(int fd, const char *path, const char *file)
{
	int wd;
	int rc = 0;
	uint32_t mask;
	struct flist f;
	char *fullpath;

	/* Skip our own directory */
	if(strcmp(file, ".tup") == 0) {
		return 0;
	}

	asprintf(&fullpath, "%s%s/", path, file);
	if(!fullpath) {
		perror("asprintf");
		return -1;
	}

	if(!isdir(fullpath))
		goto out_free;

	DEBUGP("Add watch: '%s'\n", fullpath);

	mask = IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVE;
	wd = inotify_add_watch(fd, fullpath, mask);
	if(wd < 0) {
		perror("inotify_add_watch");
		rc = -1;
		goto out_free;
	}
	dircache_add(wd, fullpath);
	flist_foreach(&f, fullpath) {
		if(strcmp(f.filename, ".") == 0 ||
		   strcmp(f.filename, "..") == 0)
			continue;
		watch_path(fd, fullpath, f.filename);
	}
	/* dircache assumes ownership of fullpath memory */
	return 0;

out_free:
	free(fullpath);
	return rc;
}

static int events_same(struct inotify_event *a, struct inotify_event *b)
{
	if(a->wd == b->wd &&
	   a->mask == b->mask &&
	   a->len == b->len) {
		if(a->len > 0) {
			return strcmp(a->name, b->name);
		}
		return 0;
	}

	return -1;
}
