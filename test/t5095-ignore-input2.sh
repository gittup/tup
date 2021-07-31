#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Updating exclusions should re-trigger rules

. ./tup.sh

cat > Tupfile << HERE
: |> sh run.sh |> output.txt
HERE
cat > run.sh << HERE
cat foo
cat bar
cat baz
touch output.txt
HERE
touch foo bar baz
update

tup_dep_exist . foo . 'sh run.sh'
tup_dep_exist . bar . 'sh run.sh'
tup_dep_exist . baz . 'sh run.sh'

cat > Tupfile << HERE
: |> sh run.sh |> output.txt ^/foo$
HERE
touch Tupfile
update

tup_dep_no_exist . foo . 'sh run.sh'
tup_dep_exist . bar . 'sh run.sh'
tup_dep_exist . baz . 'sh run.sh'

cat > Tupfile << HERE
: |> sh run.sh |> output.txt ^/foo$ ^/ba
HERE
touch Tupfile
update

tup_dep_no_exist . foo . 'sh run.sh'
tup_dep_no_exist . bar . 'sh run.sh'
tup_dep_no_exist . baz . 'sh run.sh'

cat > Tupfile << HERE
: |> sh run.sh |> output.txt
HERE
touch Tupfile
update

tup_dep_exist . foo . 'sh run.sh'
tup_dep_exist . bar . 'sh run.sh'
tup_dep_exist . baz . 'sh run.sh'
tup_object_no_exist ^ '/foo$'
tup_object_no_exist ^ '/ba'

eotup
