#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2023  Mike Shal <marfey@gmail.com>
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

# Try using a unix socket in a test case.

. ./tup.sh
check_no_windows unix socket
# OSX fails to bind() for some reason. Also the socket name is off by one
# character for some reason.
check_no_osx unix socket
check_no_freebsd unix socket

cat > unix.c <<  HERE
// Adapted from Beej's guide: http://beej.us/guide/bgipc/output/html/multipage/unixsock.html
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#define SOCK_PATH "echo_socket"

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int server_ready = 0;

static void *client(void *arg)
{
	int s, t, len;
	int rc;
	struct sockaddr_un remote;
	char str[] = "hi there";

	pthread_mutex_lock(&lock);
	while(!server_ready) {
		pthread_cond_wait(&cond, &lock);
	}
	pthread_mutex_unlock(&lock);

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	printf("Trying to connect...\\n");

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, SOCK_PATH);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	if(connect(s, (struct sockaddr *)&remote, len) < 0) {
		perror("connect");
		exit(1);
	}

	printf("Connected.\\n");

	if(send(s, str, strlen(str), 0) == -1) {
		perror("send");
		exit(1);
	}

	close(s);
	return NULL;
}

static void *server(void *arg)
{
	int s, s2, t, len, n;
	struct sockaddr_un local, remote;
	char str[100];

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, SOCK_PATH);
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	if (bind(s, (struct sockaddr *)&local, len) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(s, 5) == -1) {
		perror("listen");
		exit(1);
	}

	pthread_mutex_lock(&lock);
	server_ready = 1;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);

	printf("Waiting for a connection...\\n");
	t = sizeof(remote);
	if ((s2 = accept(s, (struct sockaddr *)&remote, &t)) == -1) {
		perror("accept");
		exit(1);
	}

	printf("Connected.\\n");

	n = recv(s2, str, 100, 0);
	if (n < 0) perror("recv");
	printf("Got message: '%.*s'\\n", n, str);
	close(s2);
	return NULL;
}

int main(void)
{
	pthread_t pid1, pid2;
	if(pthread_create(&pid1, NULL, server, NULL) < 0) {
		perror("pthread_create");
		return 1;
	}
	if(pthread_create(&pid2, NULL, client, NULL) < 0) {
		perror("pthread_create");
		return 1;
	}
	pthread_join(pid1, NULL);
	pthread_join(pid2, NULL);
	unlink(SOCK_PATH);
	return 0;
}
HERE

cat > Tupfile << HERE
: |> gcc unix.c -o %o -lpthread |> unix
: unix |> ./unix |>
HERE
update

eotup
