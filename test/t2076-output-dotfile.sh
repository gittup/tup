#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# Make sure the error message when trying to output to a .file is sane.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo > %o |> .git
HERE
tup touch Tupfile
update_fail_msg "You specified a path '.git'"

cat > Tupfile << HERE
: |> echo foo > %o |> foo/.git
HERE
tup touch Tupfile
update_fail_msg "You specified a path 'foo/.git'"

cat > Tupfile << HERE
: |> echo foo > %o |> foo/.git/baz
HERE
tup touch Tupfile
update_fail_msg "You specified a path 'foo/.git/baz'"

eotup
