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

# Try tup graph . both with and without --dirs
. ./tup.sh

tmkdir sub
cat > sub/Tupfile << HERE
.gitignore
: in.txt |> cp %f %o |> out.txt
HERE
tup touch sub/Tupfile sub/in.txt
update

tup graph . > ok.dot
tup graph --dirs . > ok-dirs.dot
gitignore_good 'node.*label=.*in.txt' ok.dot
gitignore_good 'node.*label=.*out.txt' ok.dot
gitignore_bad 'node.*label=.*sub' ok.dot
gitignore_bad 'node.*label=.*gitignore' ok.dot

gitignore_good 'node.*label=.*in.txt' ok-dirs.dot
gitignore_good 'node.*label=.*out.txt' ok-dirs.dot
gitignore_good 'node.*label=.*sub' ok-dirs.dot
gitignore_good 'node.*label=.*gitignore' ok-dirs.dot

eotup
