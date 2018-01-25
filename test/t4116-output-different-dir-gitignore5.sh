#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Make sure the generated .gitignore dir gets propagated up if there are
# multiple levels of generated dirs.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
HERE
update

tmkdir foo
cat > foo/Tupfile << HERE
.gitignore
: foreach *.c |> gcc -c %f -o %o |> ../sub/sub2/%B.o
HERE
tup touch foo/foo.c foo/bar.c
update

gitignore_good sub .gitignore

eotup
