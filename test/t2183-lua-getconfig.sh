#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2023  Mike Shal <marfey@gmail.com>
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

# Check tup.getconfig()

. ./tup.sh
cat > Tupdefault.lua << HERE
tup.rule("echo " .. tup.getconfig("FOO"))
HERE
parse
tup_object_exist . "echo "
tup_dep_exist tup.config FOO 0 .

varsetall FOO=yo
parse
tup_object_exist . "echo yo"

eotup
