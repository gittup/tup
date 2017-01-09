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

# Try to combine commands that have different numbers of incoming commands.
. ./tup.sh

cat > ok.sh << HERE
cat \$1 \$2; touch \$3
HERE
cat > Tupfile << HERE
: |> touch %o |> foo.h
: |> touch %o |> bar.h
: |> touch %o |> baz.h
: foo.h |> sh ok.sh foo.h foo.h %o |> out1.txt
: bar.h baz.h |> sh ok.sh bar.h baz.h %o |> out2.txt
HERE
tup touch Tupfile
update

tup graph . --combine > ok.dot
gitignore_good 'node.*touch.*\.h.*3 commands' ok.dot
gitignore_good 'node.*\.h.*3 files' ok.dot
gitignore_good 'node.*sh ok.sh.*2 commands' ok.dot
gitignore_good 'node.*out.*txt.*2 files' ok.dot

eotup
