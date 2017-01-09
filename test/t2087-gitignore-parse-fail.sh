#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

# .gitignore files can be created, but tup may lose track of them if a dependent
# directory is being parsed.

. ./tup.sh

tmkdir sub
cat > Tupfile << HERE
: sub/foo.c |> echo %f |>
: bork
HERE
cat > sub/Tupfile << HERE
.gitignore
: |> touch %o |> hey
HERE
tup touch Tupfile sub/Tupfile sub/foo.c
update_fail_msg 'Error parsing Tupfile'

cat > Tupfile << HERE
: sub/foo.c |> echo %f |>
HERE
tup touch Tupfile
update

eotup
