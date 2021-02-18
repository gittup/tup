#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# For some reason switching from 'gcj -C %f' to 'gcj -c %f' ends up not
# creating a link from B.java to the command for A.o, when class A uses B.
# Turns out this is because the second time around, compiling A.java reads
# from B.class instead of B.java. Then B.class is deleted (because the command
# to create it is gone, and we're creating .o files now) so the dependency is
# gone. I solved this by moving the file deletions out into its own phase.
#
# This test case merely mimics this process because when I upgraded to gcc
# 4.3.2 from 4.1.something, gcj became slow as crap. The 'mls | grep' part is
# to mimic the getdents() syscall that javac and gcj appear to use. We use a
# custom ls wrapper called "mls" because ls calls getattr() on every file on
# OSX 10.15

. ./tup.sh
check_no_windows shell

cat > mls.c << HERE
#include <stdio.h>
#include <dirent.h>

int main(void)
{
	DIR *dirp;
	struct dirent *ent;

	dirp = opendir(".");
	if(!dirp) {
		perror("opendir");
		return 1;
	}
	while((ent = readdir(dirp)) != NULL) {
		printf("%s\n", ent->d_name);
	}
	closedir(dirp);

	return 0;
}
HERE
gcc mls.c -o mls

cat > Tupfile << HERE
: B.java |> cat %f > %o |> B.class
: A.java | B.class |> (if ./mls | grep B.class > /dev/null; then echo "Using B.class"; cat B.class; else echo "Using B.java"; cat B.java; fi; cat %f) > %o |> A.class
HERE
echo "A" > A.java
echo "B" > B.java
tup touch A.java B.java Tupfile
update
check_exist A.class B.class
echo 'B' | diff - B.class
(echo 'Using B.class'; echo 'B'; echo 'A') | diff - A.class

cat > Tupfile << HERE
: B.java |> cat %f > %o |> B.o
: A.java |> sleep 0.5; (if ./mls | grep B.class > /dev/null; then echo "Using B.class"; cat B.class; else echo "Using B.java"; cat B.java; fi; cat %f) > %o |> A.o
HERE
tup touch Tupfile
update
check_not_exist A.class B.class
check_exist A.o B.o
echo 'B' | diff - B.o
(echo 'Using B.java'; echo 'B'; echo 'A') | diff - A.o

eotup
