#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

# Try a nested if.

. ./tup.sh
cat > Tupfile << HERE
ifeq (@(FOO),y)
: |> foo y |>
ifeq (@(BAR),y)
: |> foobar y |>
else
: |> foo y bar n |>
endif
else
: |> foo n |>
endif
HERE
tup touch Tupfile
varsetall FOO=y BAR=y
parse
tup_object_exist . 'foo y'
tup_object_exist . 'foobar y'
tup_object_no_exist . 'foo y bar n'
tup_object_no_exist . 'foo n'

varsetall FOO=y BAR=n
parse
tup_object_exist . 'foo y'
tup_object_no_exist . 'foobar y'
tup_object_exist . 'foo y bar n'
tup_object_no_exist . 'foo n'

varsetall FOO=n BAR=y
parse
tup_object_no_exist . 'foo y'
tup_object_no_exist . 'foobar y'
tup_object_no_exist . 'foo y bar n'
tup_object_exist . 'foo n'

varsetall FOO=n BAR=n
parse
tup_object_no_exist . 'foo y'
tup_object_no_exist . 'foobar y'
tup_object_no_exist . 'foo y bar n'
tup_object_exist . 'foo n'

eotup
