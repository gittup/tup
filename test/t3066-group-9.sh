#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2015  Mike Shal <marfey@gmail.com>
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

# When we finish processing a command, we add all destination links to the
# modify list so we can pick up where we left off if we failed. However, we
# need to make sure we don't do this with groups. We test this by setting up a
# scenario where <group> would end up in the modify list, and then remove the
# group. Since group removal just goes directly to delete_node(), this would
# leave the group in the modify list.
. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> echo hey > %o |> foo.txt | <group>
: foo.txt |> false |> bar.txt
: bar.txt |> echo bar > %o |> baz.txt | <group>
HERE
tup touch Tupfile
update_fail_msg 'failed with return value 1'

cat > Tupfile << HERE
: |> echo hey > %o |> foo.txt
: foo.txt |> true; touch %o |> bar.txt
: bar.txt |> echo bar > %o |> baz.txt
HERE
tup touch Tupfile
update

eotup
