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

# Make sure we can't try to use a ghost input dir in the parser.

. ./tup.sh
check_no_windows shell

# First make a ghost
tmkdir src
cat > src/Tupfile << HERE
: |> if [ -f ../ghost ]; then echo yes; else echo no; fi |>
HERE
update

tmkdir foo
cat > foo/Tupfile << HERE
: ../ghost/*.c |> echo %f |>
HERE
update_fail_msg "Unable to generate wildcard for directory.*since it is a ghost"

eotup
