#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Make sure we can remove a group after it's been used as an input.
. ./tup.sh

cat > Tuprules.tup << HERE
!cc = |> gcc -c %f -o %o |> %B.o | \$(MY_ROOT)/<objs>
MY_ROOT = \$(TUP_CWD)
HERE

tmkdir foo
tmkdir bar
tmkdir sub
cat > foo/Tupfile << HERE
include_rules
: foreach *.c |> !cc |>
HERE
cat > bar/Tupfile << HERE
include_rules
: foreach *.c |> !cc |>
HERE
cat > foo/main.c << HERE
int bar(void);
int main(void)
{
	return bar();
}
HERE
cat > bar/bar.c << HERE
int bar(void) {return 0;}
HERE

cat > Tupfile << HERE
: <objs> |> cat %r | xargs gcc -o %o |> myprog.exe
HERE
update

cat > Tupfile << HERE
: foo/*.o bar/*.o |> gcc %f -o %o |> myprog.exe
HERE
cat > Tuprules.tup << HERE
!cc = |> gcc -c %f -o %o |> %B.o
MY_ROOT = \$(TUP_CWD)
HERE
tup touch Tupfile Tuprules.tup
update

eotup
