#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Make sure an empty input variable doesn't generate a rule, but a blank input
# pattern does.

. ./tup.sh
cat > Tupfile << HERE
: foreach \$(srcs) |> nope |> %B.o
: \$(objs) |> not gonna work |> prog
: \$(objs) | order.h |> also not gonna work |> prog2
: | order.h |> this should work |> prog3
: |> echo foo > %o |> bar
HERE

touch Tupfile order.h
parse
tup_object_no_exist . "nope"
tup_object_no_exist . "not gonna work"
tup_object_no_exist . "also not gonna work"
tup_object_exist . "this should work"
tup_object_exist . "echo foo > bar"

eotup
