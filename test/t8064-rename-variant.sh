#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Try to rename a variant with the scanner. This will fail because the "new"
# variant has the junk in it from the old variant. Once we delete that out,
# it should get rebuilt.
. ./tup.sh

mkdir build-foo
mkdir sub

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch sub/foo.c sub/bar.c
echo "CONFIG_FOO=y" > build-foo/tup.config
update

tup_object_exist build-foo/tup.config FOO

mv build-foo build-bar
update_fail_msg "tup error: Variant directory must only contain a tup.config file"

rm -rf build-bar/sub
update

tup_object_exist build-bar/tup.config FOO
check_exist build-bar/sub/foo.o
check_exist build-bar/sub/bar.o

eotup
