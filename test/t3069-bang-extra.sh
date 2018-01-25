#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# Provide extra data to a bang rule.

. ./tup.sh
cat > Tupfile << HERE
!echo = |> echo text |>

: |> !echo |>
: |> !echo foo |>
: |> !echo bar baz |>
HERE
tup touch Tupfile
update

tup_object_exist . 'echo text'
tup_object_exist . 'echo text foo'
tup_object_exist . 'echo text bar baz'

eotup
