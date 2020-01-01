#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2020  Mike Shal <marfey@gmail.com>
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

# Check support for mkostemp

. ./tup.sh
if [ ! "$tupos" = "Linux" ]; then
	echo "mkostemp only checked under linux. Skipping test."
	eotup
fi
cat > ok.c << HERE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	char template[] = "output.XXXXXX";
	int fd;

	fd = mkostemp(template, O_APPEND);
	if(fd < 0) {
		perror("mkostemp");
		return 1;
	}
	write(fd, "text", 4);
	close(fd);
	rename(template, "output");
	return 0;
}
HERE

if gcc ok.c -o tmp 2>/dev/null; then
	rm tmp
else
	echo "[33mmkostemp not supported on this platform? Quitting successfully.[0m" 1>&2
	eotup
fi

cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> prog
: prog |> ./prog |> output
HERE
tup touch ok.c Tupfile
update

tup_dep_exist . './prog' . output

eotup
