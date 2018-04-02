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

# Test the glob match flag (%g)

. ./tup.sh
cat > Tupfile << HERE
: foreach [abc]_?ext.* |> touch %g_binary.t |> %g_binary.t
HERE
tup touch a_text.txt b_text.txt c_text.txt
parse
tup_object_exist . a_binary.t b_binary.t c_binary.t
tup upd
tup_object_exist . "touch a_binary.t"
tup_object_exist . "touch b_binary.t"
tup_object_exist . "touch c_binary.t"

eotup
