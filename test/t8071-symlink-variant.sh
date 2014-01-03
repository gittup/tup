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

# Make sure replacing a directory with a symlink works with variants.

. ./tup.sh
check_no_windows variant

mkdir foo
touch foo/foo.h

mkdir dist
cd dist
mkdir bar
cd bar
mkdir sub
cd sub
ln -s ../../../foo/foo.h
cd ../../..
mkdir build
touch build/tup.config
update

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o -Idist |> %B.o
HERE
cat > foo.c << HERE
#include "bar/sub/foo.h"
HERE
update

check_exist build/dist/bar/sub
tup_dep_exist dist/bar/sub foo.h build 'gcc -c foo.c -o foo.o -Idist'
sleep 1

rm -rf dist/bar/sub
cd dist/bar
ln -s ../../foo sub
cd ../..
update

check_not_exist build/dist/bar/sub
tup_dep_exist dist/bar sub build 'gcc -c foo.c -o foo.o -Idist'

eotup
