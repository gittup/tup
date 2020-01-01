#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2020  Mike Shal <marfey@gmail.com>
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

# Do a little torture test with various threads accessing files.
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc %f -o %o -pthread |> ok.exe
: ok.exe |> ./ok.exe |>
HERE
cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define NUM_THREADS 32
#define NUM_FILES 200

static void *thread(void *arg)
{
	int fd;
	int i;
	for(i=0; i<NUM_FILES; i++) {
		fd = open("foo.txt", O_RDONLY);
		if(fd < 0) {
			perror("foo.txt");
			return (void*)-1;
		}
		close(fd);
		fd = open("bar.txt", O_RDONLY);
		if(fd < 0) {
			perror("bar.txt");
			return (void*)-1;
		}
		close(fd);
	}
	return NULL;
}

int main(void)
{
	pthread_t pids[NUM_THREADS];
	int x;

	for(x=0; x<NUM_THREADS; x++) {
		if(pthread_create(&pids[x], NULL, thread, NULL) != 0) {
			fprintf(stderr, "pthread create error\\n");
			return 1;
		}
	}
	for(x=0; x<NUM_THREADS; x++) {
		void *res;
		if(pthread_join(pids[x], &res) != 0) {
			fprintf(stderr, "pthread join error\\n");
			return 1;
		}
		if(res != NULL)
			return 1;
	}
	return 0;
}
HERE
tup touch foo.txt bar.txt
update

eotup
