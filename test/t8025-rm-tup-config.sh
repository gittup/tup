#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Removing an in-tree tup.config shouldn't re-parse everything that
# uses an @-variable, just those that would be affected by the change.
. ./tup.sh
check_no_windows variant

tmkdir foo
tmkdir bar
cat > foo/Tupfile << HERE
ifeq (@(FOO),y)
: |> touch %o |> foo
endif
HERE
cat > bar/Tupfile << HERE
ifeq (@(BAR),y)
: |> touch %o |> bar
endif
HERE
echo "CONFIG_FOO=y" > tup.config
tup touch foo/Tupfile bar/Tupfile
update

rm tup.config
tup parse > output.txt

if ! grep foo output.txt > /dev/null; then
	echo "Error: Expected to parse 'foo'" 1>&2
	exit 1
fi

if grep bar output.txt > /dev/null; then
	echo "Error: Expected not to parse 'bar'" 1>&2
	exit 1
fi

eotup
