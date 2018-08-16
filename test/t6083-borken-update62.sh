#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018  Mike Shal <marfey@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# The ldpreload server shouldn't hang if we open files in one thread while we
# do a fork/exec in another thread. By opening a bunch of files in a loop, we
# stand a pretty good chance of having the ldpreload.so's mutex locked when the
# fork() in another thread happens.

. ./tup.sh
check_no_windows ldpreload
cat > Tupfile << HERE
: foreach *.c |> gcc %f -o %o -pthread |> %B
: ok prog |> ./ok |>
HERE
cat > prog.c << HERE
#include <stdio.h>

int main(void)
{
	printf("Ohai prog!\\n");
	return 0;
}
HERE
cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

static void *thread1(void *arg)
{
	int fd;
	int i;
	for(i=0; i<100; i++) {
		fd = open("ok.txt", O_RDONLY);
		if(fd < 0) {
			perror("ok.txt");
			return (void*)-1;
		}
		close(fd);
	}
	return NULL;
}

static void *thread2(void *arg)
{
	pid_t pid;
	pid = fork();
	if(pid == 0) {
		char *const args[] = {"prog.exe", NULL};
		execv("./prog.exe", args);
	} else {
		int status;
		if(waitpid(pid, &status, 0) < 0) {
			perror("waitpid");
			return (void*)-1;
		}
		if(WEXITSTATUS(status) != 0) {
			fprintf(stderr, "Failed to wait\\n");
			return (void*)-1;
		}
	}
	return NULL;
}

int main(void)
{
	pthread_t pid1, pid2;
	void *r1, *r2;

	if(pthread_create(&pid1, NULL, thread1, NULL) != 0) {
		fprintf(stderr, "pthread create error\\n");
		return 1;
	}
	if(pthread_create(&pid2, NULL, thread2, NULL) != 0) {
		fprintf(stderr, "pthread create error\\n");
		return 1;
	}
	if(pthread_join(pid1, &r1) != 0) {
		fprintf(stderr, "pthread join error\\n");
		return 1;
	}
	if(pthread_join(pid2, &r2) != 0) {
		fprintf(stderr, "pthread join error\\n");
		return 1;
	}
	if(r1 != NULL || r2 != NULL)
		return 1;
	return 0;
}
HERE
tup touch ok.txt
update

eotup
