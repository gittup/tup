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

# Test for node-variables getting set multiple times (through multiple
# Tuprules.tup inclusion)

. ./tup.sh

tmkdir sw
tmkdir sw/lib
tmkdir sw/app
tmkdir sw/app/core
tmkdir sw/test

cat > sw/Tuprules.tup << HERE
lib_tupfile = tup.nodevariable 'lib/Tupfile'
core_tupfile = tup.nodevariable 'app/core/Tupfile'
if vars then vars = vars .. ' sw' else vars = 'sw' end
function bang_vars(output)
	tup.definerule{command = 'echo ' .. vars .. ' > ' .. output, outputs = {output}}
end
HERE

cat > sw/lib/Tupfile << HERE
tup.dorulesfile()
if vars then vars = vars .. ' lib' else vars = 'lib' end

if tup.getcwd() == '.' then
	bang_vars 'lib.txt'
end
HERE

cat > sw/app/Tuprules.tup << HERE
if vars then vars = vars .. ' app' else vars = 'app' end
HERE

cat > sw/app/core/Tupfile << HERE
tup.dorulesfile()
tup.dofile(lib_tupfile)
if vars then vars = vars .. ' core' else vars = 'core' end

if tup.getcwd() == '.' then
	bang_vars 'core.txt'
end
HERE

cat > sw/test/Tupfile << HERE
tup.dorulesfile()
tup.dofile(core_tupfile)
if vars then vars = vars .. ' test' else vars = 'test' end
bang_vars 'test.txt'
HERE

tup touch sw/Tuprules.tup sw/lib/Tupfile sw/app/Tuprules.tup sw/app/core/Tupfile sw/test/Tupfile
update

tup_dep_exist sw/lib 'echo sw lib > lib.txt' sw/lib lib.txt
tup_dep_exist sw/app/core 'echo sw app sw lib core > core.txt' sw/app/core core.txt
tup_dep_exist sw/test 'echo sw sw app sw lib core test > test.txt' sw/test test.txt

eotup
