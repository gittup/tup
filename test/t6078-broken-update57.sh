#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# Similar to t6077, but this time we have a gen/ghost node that keeps the 'gen'
# directory from being removed by tup_db_delete_node(), and instead is pruned
# later as a ghost. In this case, the 'gen' node has the modify flag set, and
# is removed during ghost removal, which doesn't go through delete_node().
. ./tup.sh

# Windows already hits this in t6077 anyway since it makes ghost files in the
# 'gen' directory from touch.
check_no_windows shell

cat > Tupfile << HERE
: |> if [ -f gen/ghost.txt ]; then cat gen/ghost.txt; fi && touch %o |> gen/sub/output.txt
: |> touch %o |> normal.txt
: |> touch %o |> gen/sub/output2.txt
: gen/sub/*.txt |> cat %f > %o |> final.txt
HERE
update_partial normal.txt

touch Tupfile
update_partial gen/sub/output.txt

cat > Tupfile << HERE
: |> touch %o |> normal.txt
HERE
update

eotup
