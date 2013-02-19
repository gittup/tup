#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

# Same as t8072, but we forget to create the file initially.

. ./tup.sh
check_no_windows variant

mkdir build
touch build/tup.config

cat > Tupfile.lua << HERE
genfile = 'genfile.txt'
tup.definerule{outputs = {genfile}, command = 'echo generated > ' .. genfile}
tup.definerule{inputs = {genfile}, outputs = {'output.txt'}, command = 'cat ' .. genfile .. ' > output.txt'}
HERE
tup touch Tupfile.lua
update

echo 'generated' | diff - build/output.txt

cat > Tupfile.lua << HERE
genfile = 'genfile.txt'
tup.definerule{inputs = {genfile}, outputs = {'output.txt'}, command = 'cat ' .. genfile .. ' > output.txt'}
HERE
tup touch Tupfile.lua
update_fail_msg "Explicitly named file.*genfile.txt.*scheduled to be deleted"

echo 'manual' > genfile.txt
tup touch genfile.txt
update

echo 'manual' | diff - build/output.txt

eotup
