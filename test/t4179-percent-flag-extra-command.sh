#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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

# Try to use a %-flag in the extra-command section when using a !-macro
. ./tup.sh

cat > Tupfile << HERE
!foo = |> cat -v |>
: foreach *.c |> !foo "%f" > "%o" |> bin/%B
HERE
tup touch foo.c
update

tup_object_exist . 'cat -v "foo.c" > "bin/foo"'

eotup
