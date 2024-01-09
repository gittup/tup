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

# Use 'tup generate' with a build directory.

. ./tup.sh

mkdir sub
cat > sub/Tupfile << HERE
ifdef CONFIG_FOO
srcs += foo.c
endif
ifdef CONFIG_BAR
srcs += bar.c
endif
: foreach \$(srcs) |> gcc -c %f -o %o |> %B.o
HERE
touch sub/foo.c sub/bar.c

mkdir other
cat > other/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch other/baz.c other/blah.c

echo 'CONFIG_FOO=y' > foo.config
echo 'CONFIG_BAR=y' > bar.config

generate --config foo.config --builddir build $generate_script_name
./$generate_script_name

check_exist build/sub/foo.o
check_not_exist build/sub/bar.o
check_exist build/other/baz.o
check_exist build/other/blah.o

check_not_exist other/baz.o
check_not_exist other/blah.o

rm -rf build
generate --config bar.config --builddir build $generate_script_name
./$generate_script_name

check_not_exist build/sub/foo.o
check_exist build/sub/bar.o
check_exist build/other/baz.o
check_exist build/other/blah.o

eotup
