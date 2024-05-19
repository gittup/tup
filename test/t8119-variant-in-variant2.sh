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

# Like t8118, but this time both the variant and the sub variant are created at
# the same time. This goes to a slightly different error path.
. ./tup.sh

mkdir build
mkdir build/build2
mkdir sub

cat > Tupfile << HERE
srcs += baz.c
ifeq (@(FOO),y)
srcs += foo.c
endif
: foreach \$(srcs) |> gcc -c %f -o %o |> %B.o
HERE
cat > sub/Tupfile << HERE
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
echo "CONFIG_FOO=y" > build/tup.config
touch sub/bar.c baz.c foo.c
touch build/build2/tup.config

update_fail_msg "Variant directory must only contain a tup.config file. Found extra files:"

eotup
