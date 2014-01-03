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

# Test using a node-variable in a rule command line.

. ./tup.sh

tmkdir sw
tmkdir sw/toolkit
tmkdir sw/app

cat > sw/Tuprules.lua << HERE
toolkit_lib = tup.nodevariable('toolkit/toolkit.a')
HERE

cat > sw/app/Tupfile.lua << HERE
tup.definerule{command = 'cp ' .. toolkit_lib .. ' lib_copy.a', outputs = {'lib_copy.a'}}
HERE

tup touch sw/Tuprules.lua
tup touch sw/toolkit/toolkit.a
tup touch sw/app/Tupfile.lua
update

path="../toolkit/toolkit.a"
case $tupos in
	CYGWIN*)
		path="..\toolkit\toolkit.a"
		;;
esac

tup_dep_exist sw/toolkit toolkit.a sw/app "cp $path lib_copy.a"

eotup
