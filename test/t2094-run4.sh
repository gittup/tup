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

# Make sure that we only get regular files and non-deleted generated
# files when doing a readdir().
. ./tup.sh
check_no_windows run-script

cat > Tupfile << HERE
run sh ok.sh bad
: foreach *.c |> gcc -c %f -o %o |> %B.o
run sh ok.sh good
HERE
cat > ok.sh << HERE
for i in *.[co]; do
	echo ": |> echo \$1 \$i |>"
done
HERE
tup touch Tupfile ok.sh foo.c bar.c
update

tup_object_exist . 'echo good foo.c'
tup_object_exist . 'echo good bar.c'
tup_object_exist . 'echo good foo.o'
tup_object_exist . 'echo good bar.o'
tup_object_no_exist . 'echo bad foo.o'
tup_object_no_exist . 'echo bad bar.o'

tup touch Tupfile
update
tup_object_exist . 'echo good foo.c'
tup_object_exist . 'echo good bar.c'
tup_object_exist . 'echo good foo.o'
tup_object_exist . 'echo good bar.o'
tup_object_no_exist . 'echo bad foo.o'
tup_object_no_exist . 'echo bad bar.o'

eotup
