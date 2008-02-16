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
#include <stdio.h>
#include <sys/inotify.h>

int main(void)
{
	int fd;
	int wd;
	int rc = 0;
	int x;
	uint32_t mask;
	char buf[1024];

	fd = inotify_init();
	if(fd < 0) {
		perror("inotify_init");
		return -1;
	}

	mask = IN_MODIFY | IN_CREATE | IN_DELETE;
	wd = inotify_add_watch(fd, "/home/mjs/btmp", mask);
	if(wd < 0) {
		perror("inotify_add_watch");
		rc = -1;
		goto close_inot;
	}
	while((x = read(fd, buf, sizeof(buf))) > 0) {
		int offset = 0;

		while(offset < x) {
			struct inotify_event *e = (void*)buf + offset;
			printf("Received event: %x\n", e->mask);
			if(e->len > 0) {
				printf(" Name: %s\n", e->name);
			}
			offset += sizeof(*e) + e->len;
		}
	}

close_inot:
	close(fd);
	return rc;
}
