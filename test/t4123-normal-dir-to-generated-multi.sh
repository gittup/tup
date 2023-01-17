#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2023  Mike Shal <marfey@gmail.com>
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

# Similar to t4122, but with multiple levels of normal -> generated.

. ./tup.sh

mkdir foo
mkdir foo/bar
mkdir foo/bar/baz
cat > Tupfile << HERE
.gitignore
: |> touch %o |> foo/bar/baz/gen.txt
HERE
touch foo/bar/baz/ok.txt
update

gitignore_bad foo .gitignore

rm foo/bar/baz/ok.txt
update

gitignore_good foo .gitignore

check_exist foo

rm Tupfile
update

check_not_exist foo
tup_object_no_exist . foo

eotup
