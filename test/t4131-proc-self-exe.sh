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

# When doing a readlink() on /proc/self/exe in a chroot, we need to make sure
# our .tup/mnt path is not present.

. ./tup.sh
if [ ! "$tupos" = "Linux" ]; then
	echo "Only supported in Linux. Skipping test."
	eotup
fi
check_tup_suid
set_full_deps

cat > Tupfile << HERE
: |> readlink -e /proc/self/exe > %o |> file.txt
HERE
update

rlink=`which readlink`
if test -h $rlink; then
	text=`readlink -e $rlink`
else
	text=$rlink
fi
echo $text | diff - file.txt

eotup
