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

# Try a run-script from lua.

. ./tup.sh
check_no_windows run-script

cat > ok.sh << HERE
echo ': |> echo foo |>'
HERE
cat > Tupfile.lua << HERE
tup.run('sh ok.sh')
HERE
update

tup_object_exist . 'echo foo'
tup_dep_exist . Tupfile.lua 0 .
tup_dep_exist . ok.sh 0 .

eotup
