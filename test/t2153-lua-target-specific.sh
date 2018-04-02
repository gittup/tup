#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Alternate form of target-specific variables with lua.

. ./tup.sh
cat > Tupfile.lua << HERE
pages += 'foo.html'
pages += 'bar.html'
flags += '-x'
flags_foo += '-y'
tup.foreach_rule(pages, 'echo %f \$(flags) \$(flags_%B)')
HERE

tup touch Tupfile.lua foo.html bar.html
parse

tup_object_exist . 'echo foo.html -x -y'
tup_object_exist . 'echo bar.html -x '

eotup
