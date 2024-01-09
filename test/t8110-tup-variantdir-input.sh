#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2024  Mike Shal <marfey@gmail.com>
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

# Try to use TUP_VARIANTDIR as part of the input filename. Both the explicit
# TUP_VARIANTDIR based input and the one without a variant path should point to
# the same file.
. ./tup.sh

cat > Tuprules.tup << HERE
LIBS_DIR = \$(TUP_VARIANTDIR)/libs
HERE

mkdir lib
cat > lib/Tupfile << HERE
include_rules
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar crs %o %f |> \$(LIBS_DIR)/libfoo.a | \$(LIBS_DIR)/<libs>
: *.o |> ar crs %o %f |> \$(LIBS_DIR)/libbar.a | \$(LIBS_DIR)/<libs>
: \$(LIBS_DIR)/libfoo.a |> echo test1 %f > %o |> test1.txt
: ../libs/libfoo.a |> echo test2 %f > %o |> test2.txt
HERE
echo 'int foo(void) {return 7;}' > lib/foo.c

mkdir build
touch build/tup.config
update

gitignore_good '^test1 ../build/libs/libfoo.a' build/lib/test1.txt
gitignore_good '^test2 ../build/libs/libfoo.a' build/lib/test2.txt

eotup
