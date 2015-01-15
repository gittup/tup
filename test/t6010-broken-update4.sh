#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# Yet another broken update test, probably introduced from the last two. So now
# if we generate a few commands from a Tupfile (they're all modify), and the
# first one fails, so we change the Tupfile to create all new commands, the old
# ones are marked delete *and* modify, and they don't actually get removed.
# They should still be deleted.
. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o -Dbork=<Bork> |> %B.o
HERE

echo "void foo(void) {}" > foo.c
echo "void bar(void) {}" > bar.c
tup touch foo.c bar.c Tupfile
update_fail
check_not_exist foo.o bar.o

tup_object_exist . 'gcc -c foo.c -o foo.o -Dbork=<Bork>'
tup_object_exist . 'gcc -c bar.c -o bar.o -Dbork=<Bork>'

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
update
sym_check foo.o foo
sym_check bar.o bar

tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'
tup_object_no_exist . 'gcc -c foo.c -o foo.o -Dbork=<Bork>'
tup_object_no_exist . 'gcc -c bar.c -o bar.o -Dbork=<Bork>'

eotup
