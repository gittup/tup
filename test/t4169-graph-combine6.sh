#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2018  Mike Shal <marfey@gmail.com>
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

# Try to combine files that should be grouped because of combined commands.
#
# When the command nodes get combined, their edges are combined as well.
# However, if these aren't sorted when they are combined, then future nodes
# won't be combined when they should.
. ./tup.sh

cat > Tupfile << HERE
: |> cat bar.h > bar.o |> bar.o
: |> cat foo.h bar.h > foo.o |> foo.o
: foo.o |> cat foo.o |>
: |> cat bar.h foo.h > blah.h |> blah.h
HERE
tup touch foo.h bar.h Tupfile
update

tup graph . --combine > ok.dot
gitignore_good 'cat.*bar.h.*2 commands' ok.dot
gitignore_good 'foo*\.h.*2 files' ok.dot

eotup
