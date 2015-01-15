#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2015  Mike Shal <marfey@gmail.com>
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

# Make sure an in-tree build doesn't duplicate files.
. ./tup.sh
check_no_windows variant

tmkdir sub
cat > sub/ok.c << HERE
#include <stdio.h>
#include <dirent.h>

int main(void)
{
	DIR *d = opendir(".");
	struct dirent *ent;
	if(!d)
		return -1;
	while((ent = readdir(d)) != NULL) {
		printf("%s\n", ent->d_name);
	}
	closedir(d);
	return 0;
}
HERE

cat > sub/Tupfile << HERE
: ok.c |> gcc %f -o %o |> ok
: ok |> ./ok > %o |> files.txt
HERE
tup touch sub/foo.c sub/bar.c sub/Tupfile
update

if ! grep foo.c sub/files.txt | wc -l | grep 1 > /dev/null; then
	echo "Error: files.txt should only contain one foo.c" 1>&2
	cat sub/files.txt
	exit 1
fi

eotup
