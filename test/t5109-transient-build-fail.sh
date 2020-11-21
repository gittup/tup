#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2021  Mike Shal <marfey@gmail.com>
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

# Successfully create a transient file on one run when the next command fails,
# which leaves the tmp file in the transient list. Then successfully update,
# and make sure the file still gets deleted.

. ./tup.sh

cat > Tupfile << HERE
: |> ^t^ echo foo > %o |> tmp.txt
: tmp.txt |> sh run.sh |> bar.txt
HERE
cat > run.sh << HERE
exit 1
cp tmp.txt bar.txt
HERE
update_fail_msg "failed with return value 1"

check_exist tmp.txt

cat > run.sh << HERE
cp tmp.txt bar.txt
HERE
tup touch run.sh
update > .tup/tupoutput
check_not_exist tmp.txt

if grep 'echo foo' .tup/tupoutput; then
	echo "Error: Should not have re-created foo" 1>&2
	exit 1
fi

eotup
