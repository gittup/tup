#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2024  Mike Shal <marfey@gmail.com>
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

# Try to trigger an issue I had with extra_inputs referencing something in
# vardb.

. ./tup.sh

cat > rules.lua << HERE
tup.include('compile.lua')
LINUX_ROOT = tup.getcwd()
HERE
cat > compile.lua << HERE
function cc_linux(file)
	inputs = {file}
	inputs.extra_inputs += '\$(GITTUP_ROOT)/<group>'
	tup.foreach_rule(inputs, 'gcc -c %f -o %o', '%B.o')
end
HERE

cat > root.lua << HERE
tup.include('rules.lua')
cc_linux('*.c')
HERE

cat > Tuprules.tup << HERE
GITTUP_ROOT = \$(TUP_CWD)
HERE

cat > Tupfile << HERE
include_rules
include \$(GITTUP_ROOT)/root.lua
HERE

mkdir build
touch foo.c build/tup.config
update

eotup
