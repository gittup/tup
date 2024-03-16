#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2024  Bojidar Marinov <bojidar.marinov.bg@gmail.com>
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

# Make sure importing with a default value works when the environment variable
# is set the first time tup is called.

. ./tup.sh

cat > Tupfile <<HERE
.gitignore
: |> touch %o |> out/generated.txt
HERE
cat > Tupdefault <<HERE
.gitignore
HERE

update
gitignore_good out .gitignore

touch out/non-generated.txt

update
gitignore_bad out .gitignore
gitignore_good generated.txt out/.gitignore

eotup
