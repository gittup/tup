#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2014  Mike Shal <marfey@gmail.com>
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

# Combine ^o with outputs in different directories.

. ./tup.sh

cat > ok.sh << HERE
echo stringa > a/out.txt
echo stringb > b/out.txt
HERE

cat > Tupfile << HERE
: |> ^o sh ok.sh^ sh ok.sh |> a/out.txt b/out.txt
: a/out.txt |> cat a/out.txt |>
: b/out.txt |> cat b/out.txt |>
HERE
update > .output.txt

gitignore_good stringa .output.txt
gitignore_good stringb .output.txt

cat > ok.sh << HERE
echo stringa > a/out.txt
echo bstring > b/out.txt
HERE
tup touch ok.sh
update > .output.txt

gitignore_bad stringa .output.txt
gitignore_good bstring .output.txt

cat > ok.sh << HERE
echo stringa > a/out.txt
echo bstring > b/out.txt
echo cstring > c/out.txt
HERE

cat > Tupfile << HERE
: |> ^o sh ok.sh^ sh ok.sh |> a/out.txt b/out.txt c/out.txt
: a/out.txt |> cat a/out.txt |>
: b/out.txt |> cat b/out.txt |>
: c/out.txt |> cat c/out.txt |>
HERE
tup touch ok.sh Tupfile
update > .output.txt

gitignore_bad stringa .output.txt
gitignore_bad bstring .output.txt
gitignore_good cstring .output.txt

eotup
