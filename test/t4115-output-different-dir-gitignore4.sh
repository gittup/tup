#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2017  Mike Shal <marfey@gmail.com>
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

# Make sure we .gitignore generated directories, even if the generated dir
# is created after its parent has already been parsed.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
HERE

tmkdir foo
cat > foo/Tupfile << HERE
.gitignore
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo/foo.c foo/bar.c
update

gitignore_bad sub .gitignore

cat > foo/Tupfile << HERE
.gitignore
: foreach *.c |> gcc -c %f -o %o |> %B.o
: foreach *.c |> gcc -c %f -o %o |> ../sub/%B.o
HERE
tup touch foo/Tupfile
update

gitignore_good foo.o foo/.gitignore
gitignore_good bar.o foo/.gitignore
gitignore_good sub .gitignore

eotup
