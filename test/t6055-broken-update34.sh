#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# When doing a partial update, if a particular command is kept, then all of
# its outputs must also be kept. If any outputs are pruned, then those files
# aren't unlinked before running the command, causing problems with the fuse
# checks.

. ./tup.sh

cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> foo.o
: foo.o |> gcc %f -o %o && touch blah |> foo.exe | blah
HERE
echo 'int main(void) {return 0;}' > foo.c
tup touch Tupfile foo.c
update

tup touch foo.c
update foo.exe

# Try again with an output that we don't specify (we update 'foo', so
# when we go back to the command for foo.c, we have to get both 'foo.o'
# (required by 'foo'), and 'blah', which is not.
cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o && touch blah |> foo.o | blah
: foo.o |> gcc %f -o %o |> foo.exe
HERE
tup touch Tupfile
update

tup touch foo.c
update foo.exe

eotup
