#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2024  Mike Shal <marfey@gmail.com>
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

# Make sure that if we just scan the removal of a variant and re-create it with
# a different symlink.
. ./tup.sh
check_no_windows symlink

mkdir sub
mkdir configs

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch sub/foo.c sub/bar.c configs/foo.config
tup variant configs/*.config
update

rm -rf build-foo
tup scan

mkdir build-foo
touch configs/bar.config
ln -s ../configs/bar.config build-foo/tup.config
update

tup_dep_exist configs bar.config build-foo tup.config
tup_dep_no_exist configs foo.config build-foo tup.config

eotup
