#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Try changing PATH - everything should be rebuilt.

. ./tup.sh
# Windows fails with:
# gcc: fatal error: -fuse-linker-plugin, but liblto_plugin-0.dll not found
# Seems there is a problem with putting gcc in the PATH?
check_no_windows ???

tmkdir sub
cat > sub/gcc << HERE
#! /bin/sh
echo hey > foo.o
HERE
chmod +x sub/gcc

cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > foo.c
tup touch foo.c Tupfile sub/gcc
update
sym_check foo.o foo

export PATH="$PWD/sub:$PATH"
update

echo "hey" | diff - foo.o

eotup
