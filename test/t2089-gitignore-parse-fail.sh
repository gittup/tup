#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Make sure the .gitignore file isn't removed if a Tupfile fails to parse.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
: |> touch %o |> foo
HERE
tup touch Tupfile
update

gitignore_good foo .gitignore

cat > Tupfile << HERE
.gitignore
: |> touch %o |> foo
borkbork
HERE
tup touch Tupfile
update_fail_msg "Syntax error parsing Tupfile line 3"

gitignore_good foo .gitignore

eotup
