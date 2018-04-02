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

# Similar to t4122, but now we a normal directory instead of files.

. ./tup.sh

tmkdir foo
tmkdir foo/sub
cat > Tupfile << HERE
: |> touch %o |> foo/bar.txt
HERE
update

rmdir foo/sub
update > .output.txt

if ! grep 'Converting foo to a generated directory' .output.txt > /dev/null; then
	echo "Error: Expected to convert foo" 1>&2
	exit 1
fi

check_exist foo

rm Tupfile
update

check_not_exist foo
tup_object_no_exist . foo

eotup
