#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2014  Mike Shal <marfey@gmail.com>
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

# Create a circular dependency with groups incrementally. This was a problem
# since we could successfully build by adding the first rule, building, then
# adding the second rule and building. However, if we were to do a full build,
# then we would get a 'fatal tup error' because the graph wasn't empty.
. ./tup.sh

cat > Tupfile << HERE
: gen.c | <generated> |> gcc %f -o %o |> gen.exe
#: gen.exe |> ./gen.exe |> out.cpp <generated>
HERE
cat > gen.c << HERE
#include <stdio.h>

int main(void)
{
	FILE *f;
	f = fopen("out.cpp", "w");
	fprintf(f, "int x = 3;\\n");
	fclose(f);
	return 0;
}
HERE
update

cat > Tupfile << HERE
: gen.c | <generated> |> gcc %f -o %o |> gen.exe
: gen.exe |> ./gen.exe |> out.cpp <generated>
HERE
tup touch Tupfile
update_fail_msg 'both reads from and writes to this group.*generated'

cat > Tupfile << HERE
: gen.c | <generated> |> gcc %f -o %o |> gen.exe
: gen.exe |> ./gen.exe |> out.cpp
HERE
tup touch Tupfile
update

eotup
