#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2023  Mike Shal <marfey@gmail.com>
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

# Try to use readlink, both on a regular link and one that we create during
# the job.

. ./tup.sh
check_no_windows shell

cat > foo.c << HERE
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	char buf[128];
	int size;

	size = readlink("slink.txt", buf, sizeof(buf));
	if(size < 0) {
		perror("readlink");
		return 1;
	}
	if(strncmp(buf, "target.txt", 10) != 0) {
		printf("readlink doesn't match target :(\n");
		return 1;
	}
	return 0;
}
HERE
touch target.txt
ln -s target.txt slink.txt
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ./foo |>
HERE
update

rm slink.txt
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ln -s target.txt slink.txt && ./foo |> slink.txt
HERE
update

eotup
