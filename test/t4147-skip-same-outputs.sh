#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Sometimes we may want to skip further processing if a command produces
# the same output as last time. Since this requires essentially diffing
# the old outputs vs the new, it requires a per-command flag.

. ./tup.sh

cat > ok.sh << HERE
echo stringa > a
echo stringb > b
echo stringc > c
HERE

cat > Tupfile << HERE
: |> ^o sh ok.sh^ sh ok.sh |> a b c
: a |> cat a |>
: b |> cat b |>
: c |> cat c |>
HERE
update > .output.txt

gitignore_good stringa .output.txt
gitignore_good stringb .output.txt
gitignore_good stringc .output.txt

cat > ok.sh << HERE
echo stringa > a
echo stringb > b
echo cstring > c
HERE
touch ok.sh
update > .output.txt

gitignore_bad stringa .output.txt
gitignore_bad stringb .output.txt
gitignore_good cstring .output.txt

eotup
