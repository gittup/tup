#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Extra inputs for lua with %b.

. ./tup.sh
cat > Tupfile.lua << HERE
pages += 'foo.html'
pages += 'bar.html'
pages.extra_inputs += 'menu.inc'
tup.rule(nil, 'echo hey > %o', 'menu.inc')
tup.foreach_rule(pages, 'cp %f %o', '%b.gen')
HERE

tup touch Tupfile.lua foo.html bar.html hey.txt
parse

tup_object_exist . 'cp foo.html foo.html.gen'
tup_object_exist . 'cp bar.html bar.html.gen'

tup_sticky_exist . 'menu.inc' . 'cp foo.html foo.html.gen'
tup_sticky_exist . 'menu.inc' . 'cp bar.html bar.html.gen'

eotup
