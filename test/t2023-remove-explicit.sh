#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2013  Mike Shal <marfey@gmail.com>
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

# Make sure we can remove an explicit dependency.

. ./tup.sh
cat > Tupfile << HERE
: |> touch %o |> foo.c
: |> touch %o |> bar.h
: foo.c | bar.h |> cat %f |>
HERE
tup touch Tupfile
tup parse

tup_sticky_exist . foo.c . 'cat foo.c'
tup_sticky_exist . bar.h . 'cat foo.c'

cat > Tupfile << HERE
: |> touch %o |> foo.c
: |> touch %o |> bar.h
: foo.c |> cat %f |>
HERE
tup touch Tupfile
tup parse
tup_sticky_exist . foo.c . 'cat foo.c'
tup_sticky_no_exist . bar.h . 'cat foo.c'

eotup
