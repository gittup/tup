#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Try to readdir() from a run-script on a subdirectory.
. ./tup.sh

cat > Tupfile << HERE
run sh -e ok.sh
HERE
cat > ok.sh << HERE
for i in sub/*.[co]; do
	echo ": |> echo \$i |>"
done
HERE

tmkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile ok.sh sub/foo.c sub/bar.c
update_fail_msg 'Fuse server reported an access violation'

# TODO: Allow readdir() to parse subdirs automatically? Would cause a loop
# in fuse, so multi-threaded fuse may be required.
eotup #TODO

tup_object_exist . 'echo sub/foo.c'
tup_object_exist . 'echo sub/bar.c'
tup_object_exist . 'echo sub/foo.o'
tup_object_exist . 'echo sub/bar.o'

eotup
