#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2022  Mike Shal <marfey@gmail.com>
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

# Similar to t4147, but with symlinks.

. ./tup.sh
check_no_windows symlink

cat > ok.sh << HERE
ln -s linka a
ln -s linkb b
ln -s linkc c
HERE

cat > Tupfile << HERE
: |> ^o sh ok.sh^ sh ok.sh |> a b c
: a |> readlink a |>
: b |> readlink b |>
: c |> readlink c |>
HERE
update > .output.txt

gitignore_good linka .output.txt
gitignore_good linkb .output.txt
gitignore_good linkc .output.txt

cat > ok.sh << HERE
ln -s linka a
ln -s linkb b
ln -s clink c
HERE
update > .output.txt

gitignore_bad linka .output.txt
gitignore_bad linkb .output.txt
gitignore_good clink .output.txt

eotup
