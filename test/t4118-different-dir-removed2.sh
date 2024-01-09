#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# Check to make sure that generated dirs are correctly updated when their
# parent dir is removed and then scanned.

. ./tup.sh

mkdir foo
cat > ok.sh << HERE
if [ -f foo/ok.txt ]; then echo yes; else echo no; fi
HERE
cat > Tupfile << HERE
: |> echo hey > %o |> foo/bar/new/baz.txt <txt>
: |> sh ok.sh |>
HERE
mkdir sub
cat > sub/Tupfile << HERE
: ../<txt> |> cat ../foo/bar/new/baz.txt |>
HERE
update

rm -rf foo
# Separate scan is needed for the test to fail. When things weren't working,
# the first scan would move some nodes to ghosts, but not the generated dirs.
# On the second scan, generated dirs would then not have a valid parent since
# their parent is a ghost.
tup scan
update

eotup
