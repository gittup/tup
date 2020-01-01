#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# When the parser version updates, make sure all Tupfiles are re-parsed.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo
HERE
tmkdir sub
cat > sub/Tupfile << HERE
: |> touch %o |> bar
HERE
tup touch Tupfile sub/Tupfile
update

check_exist foo sub/bar
tup fake_parser_version

cat > Tupfile << HERE
: |> touch %o |> foo
: |> touch %o |> foo2
HERE
cat > sub/Tupfile << HERE
: |> touch %o |> bar
: |> touch %o |> bar2
HERE

# Don't tup touch the Tupfiles - the parser version should cause them to update
update > .tupoutput
if ! grep 'Tup parser version has been updated' .tupoutput > /dev/null; then
	echo "*** Expected the parser version update message to be displayed, but it wasn't." 1>&2
	exit 1
fi
check_exist foo2 sub/bar2

eotup
