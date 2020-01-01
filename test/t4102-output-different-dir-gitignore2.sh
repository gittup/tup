#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2020  Mike Shal <marfey@gmail.com>
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

# Make sure creating foo/.gitignore before we write another file there still
# results in the correct .gitignore file.

. ./tup.sh

tmkdir foo
cat > foo/Tupfile << HERE
.gitignore
: |> echo foo > %o |> baz.txt
HERE
tup touch foo/Tupfile
update

cat > Tupfile << HERE
: |> echo hey > %o |> foo/bar.txt
HERE
tup touch Tupfile
update

gitignore_good baz.txt foo/.gitignore
gitignore_good bar.txt foo/.gitignore

if ! cat foo/.gitignore | grep '\.gitignore' | wc -l | grep 1 > /dev/null; then
	echo "Error: Expected only one .gitignore line in the .gitignore file." 1>&2
	exit 1
fi

eotup
