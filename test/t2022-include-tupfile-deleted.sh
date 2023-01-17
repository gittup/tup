#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# See if we delete an included Tupfile that we get re-parsed.

. ./tup.sh
cat > Tupfile << HERE
include yo/Install.tup
: |> echo foo |>
HERE

mkdir yo
cat > yo/Install.tup << HERE
: |> echo bar |>
HERE

update
tup_object_exist . 'echo foo'
tup_object_exist . 'echo bar'

rm yo/Install.tup
rmdir yo

cat > Tupfile << HERE
: |> echo foo |>
HERE
# Note: Do not 'touch Tupfile' - want to see if rming Install.tup causes it
# to be re-parsed. But I also want to parse it successfully so I can see the
# command gets removed.

update
tup_object_exist . 'echo foo'
tup_object_no_exist . 'echo bar'

eotup
