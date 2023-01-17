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

# Exporting a variable that does not exist should also not exist for the
# sub-process.

. ./tup.sh

cat > foo.c << HERE
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	char *val;
	val = getenv("TUP_TEST_FOO");
	if(val == NULL)
		printf("not found\n");
	else
		printf("Got value: %s\n", val);
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo.exe
export TUP_TEST_FOO
: foo.exe |> ./%f > %o |> output.txt
HERE
update

echo "not found" | diff - output.txt

export TUP_TEST_FOO="test"
update
echo "Got value: test" | diff - output.txt

unset TUP_TEST_FOO
update
echo "not found" | diff - output.txt

eotup
