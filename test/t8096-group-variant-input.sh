#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Like t3071, but in a variant.
. ./tup.sh
check_no_windows variant

cat > Tuprules.tup << HERE
MY_ROOT = \$(TUP_CWD)
HERE

tmkdir foo
tmkdir bar
cat > foo/Tupfile << HERE
include_rules
: foreach *.c |> gcc -c %f -o %o |> %B.o | \$(MY_ROOT)/<objs>
HERE
cat > bar/Tupfile << HERE
include_rules
: foreach *.c |> gcc -c %f -o %o |> %B.o | \$(MY_ROOT)/<objs>
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

# Link all the outputs from the root.
cat > Tupfile << HERE
: <objs> |> gcc %<objs> -o %o |> myprog.exe
HERE

# Same, but now we need the ../ path to get to the objs.
tmkdir linked
cat > linked/Tupfile << HERE
include_rules
: \$(MY_ROOT)/<objs> |> gcc %<objs> -o %o |> myprog.exe
HERE
update

# Make sure if we add a new file to the group, the things using the group
# are re-executed.
echo 'int marfx;' > foo/newfoo.c
tmkdir build
tup touch foo/newfoo.c build/tup.config
update

sym_check build/myprog.exe marfx
sym_check build/linked/myprog.exe marfx

eotup
