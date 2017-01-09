#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Like t2011 and t2018, but this time the prerequisite gets deleted later.

. ./tup.sh
cat > Tupfile << HERE
: foo.txt |> cat %f > %o |> bar.txt
: bar.txt |> cat %f |>
HERE

echo hey > foo.txt
tup touch Tupfile foo.txt
update

cat > Tupfile << HERE
: bar.txt |> cat %f |>
HERE
tup touch Tupfile
update_fail

eotup
