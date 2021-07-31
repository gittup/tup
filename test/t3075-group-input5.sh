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

# Similar to t3074, but we just remove an output from a command, rather
# than removing the group from a command.
. ./tup.sh

check_list()
{
	if ! grep $1 $2 > /dev/null; then
		echo "Error: Expected to find $1 in $2" 1>&2
		cat $2
		exit 1
	fi
}

cat > Tuprules.tup << HERE
MY_ROOT = \$(TUP_CWD)
HERE

mkdir foo
mkdir bar
cat > foo/Tupfile << HERE
include_rules
: |> touch %o |> foo.txt | \$(MY_ROOT)/<txt>
HERE
cat > bar/Tupfile << HERE
include_rules
: |> touch %o |> bar.txt | \$(MY_ROOT)/<txt>
HERE

cat > Tupfile << HERE
: <txt> |> cat %<txt>.res > %o |> mylist.txt
HERE
update

check_list foo/foo.txt mylist.txt
check_list bar/bar.txt mylist.txt

cat > foo/Tupfile << HERE
include_rules
: |> touch %o |> foo.txt newfoo.txt | \$(MY_ROOT)/<txt>
HERE
touch foo/Tupfile
update

check_list foo/foo.txt mylist.txt
check_list bar/bar.txt mylist.txt
check_list foo/newfoo.txt mylist.txt

cat > foo/Tupfile << HERE
include_rules
: |> touch %o |> foo.txt | \$(MY_ROOT)/<txt>
HERE
touch foo/Tupfile
update

check_list foo/foo.txt mylist.txt
check_list bar/bar.txt mylist.txt

if grep foo/newfoo.txt mylist.txt > /dev/null; then
	echo "Error: Expected foo/newfoo.txt to be removed from the list." 1>&2
	exit 1
fi

eotup
