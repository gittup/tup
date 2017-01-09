#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2017  Mike Shal <marfey@gmail.com>
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

# Make sure OSX updates properly if exchangedata is used to write out a file.

. ./tup.sh

if [ "$tupos" != "Darwin" ]; then
	echo "[33mTest only supported on OSX[0m"
	eotup
fi

cat > save.c << HERE
#include <stdio.h>
#include <unistd.h>
#include <sys/attr.h>

int main(void)
{
	FILE *fin;
	FILE *fout;
	char buf[1024];
	int n;
	fin = fopen("foo.c", "r");
	fout = fopen(".tmp.txt", "w");
	if(!fin || !fout) {
		return -1;
	}
	while((n = fread(buf, 1, 1024, fin)) > 0) {
		fwrite(buf, 1, n, fout);
	}
	fwrite("int y;\\n", 1, 7, fout);
	fclose(fin);
	fclose(fout);
	exchangedata(".tmp.txt", "foo.c", 0);
	return 0;
}
HERE
gcc save.c -o save

echo 'int x;' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
update

sym_check foo.o x ^y

sleep 1
./save
update

sym_check foo.o x y

eotup
