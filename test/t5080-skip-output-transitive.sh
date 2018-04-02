#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2018  Mike Shal <marfey@gmail.com>
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

# Similar to t5070, but with ^o^ flags to skip outputs.

. ./tup.sh

cat > ok1.sh << HERE
cat foo
HERE

cat > ok2.sh << HERE
cat foo
cat bar
HERE

chmod +x ok1.sh ok2.sh

cat > Tupfile << HERE
: |> ^o^ echo blah > %o |> foo
: foo |> ^o^ ./ok1.sh > %o |> bar
: bar |> ^o^ ./ok2.sh |>
HERE
update

cat > ok1.sh << HERE
echo blah
HERE
tup touch ok1.sh
update_fail_msg "Missing input dependency"

eotup
