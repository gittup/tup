#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Try truncate.

. ./tup.sh
check_no_windows truncate

cat > foo.c << HERE
#include <unistd.h>
#include <sys/types.h>

int main(void)
{
	if(truncate("tmp.txt", 4) < 0) {
		perror("tmp.txt");
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> (echo hey; echo there) > %o; ./foo |> tmp.txt
HERE
tup touch Tupfile foo.c
update

echo 'hey' | diff - tmp.txt
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
HERE
tup touch Tupfile
update
check_not_exist tmp.txt

cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ./foo |>
HERE
tup touch Tupfile tmp.txt
update_fail_msg "tup error.*truncate"

eotup
