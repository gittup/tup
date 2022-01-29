#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2022  Mike Shal <marfey@gmail.com>
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

# Since the top-level .gitignore file ignores the '.tup' directory, we should
# make sure one exists if there is a top-level Tuprules.tup that has
# '.gitignore' even if there is no top-level Tupfile. Note that we should only
# pick up the .gitignore directive, not any rules.

. ./tup.sh

cat > Tuprules.tup << HERE
.gitignore
: |> echo hey |>
HERE
mkdir sub
cat > sub/Tupfile << HERE
include_rules
: |> touch foo |> foo
HERE
mkdir sub2
cat > sub2/Tupfile << HERE
: |> touch bar |> bar
HERE
update

gitignore_good .tup .gitignore
gitignore_good foo sub/.gitignore
check_not_exist bar/.gitignore
tup_object_no_exist . 'echo hey'
tup_dep_exist . 'Tuprules.tup' 0 .

# Make sure we lose the dep on Tuprules.tup if we create a real Tupfile with no
# include_rules
cat > Tupfile << HERE
HERE
update
tup_dep_no_exist . 'Tuprules.tup' 0 .

eotup
