#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# The ghost nodes must be creating before the links are written to them, but if
# an error occurs while writing the links (such as due to a missing input
# dependency), then the ghost nodes are never connected. If the command changes
# such that it no longer reads from the ghost, then the ghost will persist.

. ./tup.sh

cat > Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
: |> sh ok.sh |>
HERE
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; fi
cat foo.h
HERE

update_fail

cat > ok.sh << HERE
echo yo
HERE

update
tup_object_no_exist . ghost

eotup
