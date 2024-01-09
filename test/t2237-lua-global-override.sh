#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022-2024  Mike Shal <marfey@gmail.com>
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

# Make sure we can override a "global" variable (eg: a global table like
# 'string') with our internal namespace, but not have it impact other Tupfiles.

. ./tup.sh
mkdir a
mkdir b
cat > a/Tupfile.lua << HERE
string = 'foo'
tup.rule("echo \$(string)")
HERE
cat > b/Tupfile.lua << HERE
tup.rule("echo " .. string.gsub("foo", "o", "u"))
HERE
update

tup_object_exist a 'echo foo'
tup_object_exist b 'echo fuu'

eotup
