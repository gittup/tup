#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021  Mike Shal <marfey@gmail.com>
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

# Make sure the progress bar is accurate when deleting or converting multiple
# files.

. ./tup.sh
cat > .tup/options << HERE
[display]
job_numbers = 1
HERE

cat > Tupfile << HERE
: foreach in*.txt |> cp %f %o |> %B.out.txt
HERE
for i in `seq 1 8`; do touch in$i.txt; done
update

for i in `seq 1 4`; do touch in$i.out.txt; done
echo "" > Tupfile
update > .tup/output.txt

if ! grep '0) rm: ' .tup/output.txt > /dev/null; then
	cat .tup/output.txt
	echo "Error: Never got to 0 when removing files." 1>&2
	exit 1
fi

eotup
