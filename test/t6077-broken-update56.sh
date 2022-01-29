#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2022  Mike Shal <marfey@gmail.com>
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

# This one is a bit complicated. First we create a generated dir in the parser
# ('gen'), but don't write any files to it yet. On the second update, since the
# directory doesn't exist yet, it gets converted temporarily to a ghost, but
# then immediately goes back to a generated directory since the Tupfile is
# re-parsed. Since it was briefly a ghost, the directory is in the modify list.
# We create one of the files in the generated directory during the second
# update, but ensure that the directory itself is not processed during
# update_work() so it stays in the modify list. During the third update, we
# have the generated directory in the modify list, but now the rules to create
# it are removed. The logic to remove generated directories skips the
# unflagging, so after the directory is removed it remains in the modify list,
# which causes a failure to find the node entry during update.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> gen/output.txt
: |> touch %o |> normal.txt
: |> touch %o |> gen/output2.txt
: gen/*.txt |> cat %f > %o |> final.txt
HERE
update_partial normal.txt

touch Tupfile
update_partial gen/output.txt

cat > Tupfile << HERE
: |> touch %o |> normal.txt
HERE
update

eotup
