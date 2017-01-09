#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2017  Mike Shal <marfey@gmail.com>
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

# Try the !tup_ln macro in lua

. ./tup.sh

cat > Tupfile.lua << HERE
tup.foreach_rule('*.txt', '!tup_ln', '%B.lnk')
HERE
tup touch foo.txt bar.txt
update

tup_dep_exist . "$(tup_ln_cmd foo.txt foo.lnk)" . foo.lnk
tup_dep_exist . "$(tup_ln_cmd bar.txt bar.lnk)" . bar.lnk

eotup
