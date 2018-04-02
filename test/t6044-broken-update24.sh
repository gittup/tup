#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2018  Mike Shal <marfey@gmail.com>
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

# Make sure hidden files are still tracked internally during command
# execution, even though they won't make it into the final DAG.
. ./tup.sh
check_no_windows shell

if grep 'Fedora' /etc/system-release > /dev/null; then
	echo "[33mSkipping on Fedora because coreutils' mv uses a syscall directly instead of renameat2[0m"
	eotup
fi

cat > Tupfile << HERE
: |> touch .foo; mv .foo bar |> bar
HERE
tup touch Tupfile
update

update > tmp.txt
if grep 'touch .foo' tmp.txt > /dev/null; then
	cat tmp.txt
	echo "Error: Expected second update not to run the command." 1>&2
	exit 1
fi

eotup
