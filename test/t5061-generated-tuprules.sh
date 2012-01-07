#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Make sure we can't sneak in a generated Tuprules.tup file by making it a
# ghost node first and then generating it later.
. ./tup.sh
tmkdir sub
cat > sub/Tupfile << HERE
include_rules
CFLAGS += foo
: |> echo \$(CFLAGS) |>
HERE
tup touch sub/Tupfile
update
tup_object_exist sub 'echo foo'

cat > Tupfile << HERE
: |> echo 'CFLAGS += bar' > %o |> Tuprules.tup
HERE
tup touch Tupfile
update_fail

eotup
