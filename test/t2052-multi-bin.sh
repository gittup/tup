#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# Multiple output bins don't make sense, so make sure those fail.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c bar.c |> gcc -c %f -o %o |> %B.o {objs} {blah}
HERE
tup touch foo.c bar.c Tupfile
parse_fail_msg "bin must be at the end of the output list"

eotup
