#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2017-2023  Mike Shal <marfey@gmail.com>
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

# With a generated directory under a normal directory, then removing the normal
# directory, it is possible to end with a generated directory node in the
# create list. This shouldn't cause tup to complain about unknown node types
# being in the create graph.
. ./tup.sh

mkdir outdir
touch outdir/foo.txt
cat > Tupfile << HERE
: |> touch %o |> outdir/sub/bar.txt
: outdir/sub/bar.txt |> cp %f %o |> outdir/sub/baz.txt
HERE

update

rm -rf outdir
update

eotup
