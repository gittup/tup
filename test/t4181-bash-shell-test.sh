#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016  Mike Shal <marfey@gmail.com>
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

# Test that $BASH is set in bash
. ./tup.sh
check_bash

cat > Tupfile << HERE
: |> ^b^ echo \$BASH > %o |> bash.txt
HERE
tup touch Tupfile
update

check_exist bash.txt

# Check that bash is not "empty" i.e. $BASH was defined
if echo | cmp - bash.txt > /dev/null 2>&1; then
	echo "[31m*** \$BASH was undefined in bash :-rule.[0m"
    exit 1
fi

eotup
