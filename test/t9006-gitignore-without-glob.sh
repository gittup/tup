#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# Try to use the .gitignore directive

. ./tup.sh
cat > Tuprules.tup << HERE
.gitignore
HERE

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> [asd].exe
include_rules
HERE
echo 'int main(void) {return 0;}' > foo.c

tup touch foo.c bar.c Tupfile Tuprules.tup
update

gitignore_good '/\\\[asd\\\].exe' .gitignore

eotup
