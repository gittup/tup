/*
 * I humbly present the Love-Trowbridge (Lovebridge?) recursive directory
 * scanning algorithm:
 *
 *        Step 1.  Start at initial directory foo.  Add watch.
 *        
 *        Step 2.  Setup handlers for watch created in Step 1.
 *                 Specifically, ensure that a directory created
 *                 in foo will result in a handled CREATE_SUBDIR
 *                 event.
 *        
 *        Step 3.  Read the contents of foo.
 *        
 *        Step 4.  For each subdirectory of foo read in step 3, repeat
 *                 step 1.
 *        
 *        Step 5.  For any CREATE_SUBDIR event on bar, if a watch is
 *                 not yet created on bar, repeat step 1 on bar.
 */

/* _GNU_SOURCE for asprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "dircache.h"
#include "flist.h"
#include "debug.h"
#include "tupid.h"
#include "mkdirhier.h"
#include "tup-compat.h"

static int isdir(const char *file);
static int watch_path(const char *path, const char *file);
static void handle_event(struct inotify_event *e);
static int events_same(struct inotify_event *a, struct inotify_event *b);
static int create_tup_file(const char *file);

static int inot_fd;

int main(int argc, char **argv)
{
	int x;
	int rc = 0;
	struct timeval t1, t2;
	const char *path = NULL;
	static char buf[(sizeof(struct inotify_event) + 16) * 1024];
	static char last[sizeof(struct inotify_event) + PATH_MAX];

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
	inot_fd = inotify_init();
	if(inot_fd < 0) {
		perror("inotify_init");
		return -1;
	}

	if(watch_path(path, "") < 0) {
		rc = -1;
		goto close_inot;
	}

	gettimeofday(&t2, NULL);
	fprintf(stderr, "Initialized in %f seconds.\n",
		(double)(t2.tv_sec - t1.tv_sec) +
		(double)(t2.tv_usec - t1.tv_usec)/1e6);

	while((x = read(inot_fd, buf, sizeof(buf))) > 0) {
		int offset = 0;

		while(offset < x) {
			struct inotify_event *e = (void*)((char*)buf + offset);

			/* Skip duplicate events (ie: for a program writing to
			 * a file, the IN_MODIFY event will be received
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

			handle_event(e);
next_event:
			offset += sizeof(*e) + e->len;
		}
	}

close_inot:
	close(inot_fd);
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

static int watch_path(const char *path, const char *file)
{
	int wd;
	int rc = 0;
	int len;
	uint32_t mask;
	struct flist f;
	char *fullpath;

	/* Skip our own directory */
	if(strcmp(file, ".tup") == 0) {
		return 0;
	}

	len = asprintf(&fullpath, "%s%s/", path, file);
	if(len < 0) {
		perror("asprintf");
		return -1;
	}

	if(!isdir(fullpath)) {
		/* Remove trailing / */
		fullpath[len-1] = 0;
		create_tup_file(fullpath);
		goto out_free;
	}

	DEBUGP("Add watch: '%s'\n", fullpath);

	mask = IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVE;
	wd = inotify_add_watch(inot_fd, fullpath, mask);
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
		watch_path(fullpath, f.filename);
	}
	/* dircache assumes ownership of fullpath memory */
	return 0;

out_free:
	free(fullpath);
	return rc;
}

static void handle_event(struct inotify_event *e)
{
	DEBUGP("Event: wd=%i, name='%s'\n", e->wd, e->name);

	/* TODO: Handle MOVED_FROM/MOVED_TO events */
	if(e->len > 0) {
		printf("%08x:%s%s\n", e->mask,
		       dircache_lookup(e->wd), e->name);
	} else {
		printf("%08x:%s\n", e->mask,
		       dircache_lookup(e->wd));
	}
	if(e->mask & IN_CREATE) {
		if(e->mask & IN_ISDIR) {
			watch_path(dircache_lookup(e->wd), e->name);
		} else {
			create_tup_file(e->name);
		}
	}
	if(e->mask & IN_IGNORED) {
		dircache_del(e->wd);
	}
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

static int create_tup_file(const char *file)
{
	int fd;
	int rc;
	int len;
	char tupfilename[] = ".tup/" SHA1_X "/tup.name";
	static char read_filename[PATH_MAX];

	file = tupid_from_filename(tupfilename + 5, file);

	DEBUGP("Create tup file '%s' containing '%s'.\n", tupfilename, file);
	if(mkdirhier(tupfilename) < 0)
		return -1;
        fd = open(tupfilename, O_RDONLY);
        if(fd < 0) {
                fd = open(tupfilename, O_WRONLY | O_CREAT, 0666);
                if(fd < 0) {
                        perror("open");
                        return -1;
                }
                len = strlen(file);
                rc = write(fd, file, len);
                if(rc < 0) {
                        perror("write");
                        return -1;
                }
                if(rc != len) {
                        fprintf(stderr, "Unable to write all %i bytes to %s.\n",
                                len, tupfilename);
                }
        } else {
                rc = read(fd, read_filename, sizeof(read_filename) - 1);
                if(rc < 0) {
                        perror("read");
                        return -1;
                }
                read_filename[rc] = 0;
                if(strcmp(read_filename, file) != 0) {
                        fprintf(stderr, "Gak! SHA1 collision? Requested "
                                "file '%s' doesn't match stored file '%s' for "
                                "in '%s'\n", file, read_filename, tupfilename);
                        return -1;
                }
        }
        close(fd);
        return 0;
}
