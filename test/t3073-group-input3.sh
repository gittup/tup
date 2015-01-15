#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2015  Mike Shal <marfey@gmail.com>
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

# Make sure we only get the group we ask for.
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
: |> echo hey > %o |> tmp.txt | \$(MY_ROOT)/<txt>
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
: <txt> <objs> |> gcc %<objs> -o %o |> myprog.exe
HERE
update

tup_dep_exist . '<objs>' . 'gcc %<objs> -o myprog.exe'
tup_dep_no_exist . '<txt>' . 'gcc %<objs> -o myprog.exe'

eotup
