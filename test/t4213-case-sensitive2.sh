#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2022  Mike Shal <marfey@gmail.com>
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

# Make sure things like rename and unlink work properly when changing the case
# of file accesses in Windows

. ./tup.sh
check_windows case-sensitive

cat > run-unlink.sh << HERE
touch foo.txt
cat FOO.txt
rm FOO.TXT
HERE

cat > run-unlink2.sh << HERE
touch u2.txt
rm U2.txt
touch u2.TXT
HERE

cat > rename.c << HERE
#include <stdio.h>
int main(void)
{
	return rename("FOO-TMP.TXT", "fooout.txt");
}
HERE
gcc rename.c -o rename
cat > run-rename.sh << HERE
touch foo-tmp.txt
cat FOO-tmp.txt
./rename
HERE

cat > run-mv.sh << HERE
touch bar-tmp.txt
cat bar-tmp.txt
mv BAR-TMP.TXT barout.txt
HERE

cat > Tupfile << HERE
: |> sh run-unlink.sh |>
: |> sh run-unlink2.sh |> u2.txt
: |> sh run-rename.sh |> fooout.txt
: |> sh run-mv.sh |> barout.txt
HERE
update

tup_object_no_exist . foo.txt
tup_object_no_exist . FOO.txt
tup_object_no_exist . FOO.TXT
tup_object_no_exist . foo-tmp.txt
tup_object_no_exist . bar-tmp.txt

eotup
