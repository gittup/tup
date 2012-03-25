#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Test using a node-variable in a runscript line

. ./tup.sh

tmkdir sw
tmkdir tools

cat > Tuprules.tup << HERE
script %= tools/script.sh
HERE

cat > sw/Tupfile << HERE
include_rules
run %(script)
HERE

cat > tools/script.sh << HERE
#!/bin/sh
echo ": |> touch %o |> a.txt"
HERE

chmod u+x tools/script.sh

tup touch Tuprules.tup
tup touch sw/Tupfile
tup touch tools/script.sh
update

tup_dep_exist tools script.sh . sw
tup_dep_exist sw 'touch a.txt' sw a.txt

eotup
