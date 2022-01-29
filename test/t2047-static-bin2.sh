#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# See if we can use separate rules to go into the same bin. Note I have no idea
# if the syntax for 'as' is correct - I'm just checking the parser.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o {objs}
: bar.S |> as %f -o %o |> %B.o {objs}
: {objs} |> gcc %f -o %o |> prog
HERE
touch foo.c bar.S
parse
tup_sticky_exist . 'foo.o' . 'gcc foo.o bar.o -o prog'
tup_sticky_exist . 'bar.o' . 'gcc foo.o bar.o -o prog'

# Re-order the first two rules.
cat > Tupfile << HERE
: bar.S |> as %f -o %o |> %B.o {objs}
: foo.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} |> gcc %f -o %o |> prog
HERE
# Parse here to remove the old command
parse
tup_object_no_exist . 'gcc foo.o bar.o -o prog'
tup_sticky_exist . 'foo.o' . 'gcc bar.o foo.o -o prog'
tup_sticky_exist . 'bar.o' . 'gcc bar.o foo.o -o prog'

eotup
