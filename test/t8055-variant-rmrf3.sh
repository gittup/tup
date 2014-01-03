#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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

# Make sure that if we just scan the removal of a variant and re-create it, things
# still work.
. ./tup.sh
check_no_windows variant

tmkdir sub
tmkdir configs

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch sub/Tupfile sub/foo.c sub/bar.c configs/foo.config
tup variant configs/*.config
update

rm -rf build-foo
tup scan

tup variant configs/*.config
update

check_exist build-foo/sub/foo.o
check_exist build-foo/sub/bar.o

eotup
