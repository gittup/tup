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

# Like t6021, but this time we re-order things so the 'output' file is marked
# as delete when we parse the 'cat output' rule so we don't get yelled at.
# However, the command needs to be re-executed in order to see that the output
# file is no longer necessary (since here, it is).

. ./tup.sh
cat > Tupfile << HERE
: |> echo foo > %o |> output
: output |> cat output |>
HERE
update

cat > Tupfile << HERE
: |> cat output |>
: |> echo foo > %o |> output
HERE
update_fail_msg "Missing input dependency"

cat > Tupfile << HERE
: output |> cat output |>
: |> echo foo > %o |> output
HERE
update_fail_msg "Explicitly named file 'output' in subdir '.' is scheduled to be deleted"

cat > Tupfile << HERE
: |> echo foo > %o |> output
: output |> cat output |>
HERE
update

eotup
