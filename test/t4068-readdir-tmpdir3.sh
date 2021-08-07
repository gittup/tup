#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# Verify that readdir will correctly list nested tmp subdirectories.

. ./tup.sh
check_no_freebsd TODO

mkdir sub
cd sub
cat > ok.sh << HERE
mkdir tmpsub
mkdir tmpsub/level1
mkdir tmpsub/level2
mkdir tmpsub/level1/sublevel1.1
mkdir tmpsub/level1/sublevel1.2
mkdir tmpsub/level2/sublevel1.1
ls tmpsub 2>/dev/null
rm -r tmpsub
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
cd ..
update

for i in level1 level2; do
	if ! grep "$i" sub/output.dat > /dev/null; then
		echo "Error: '$i' should be in the output file" 1>&2
		exit 1
	fi
done
if grep sublevel sub/output.dat > /dev/null; then
	echo "Error: sublevel dirs should not be in the output file yet" 1>&2
	exit 1
fi

cat > ok.sh << HERE
mkdir tmpsub
mkdir tmpsub/level1
mkdir tmpsub/level2
mkdir tmpsub/level1/sublevel1.1
mkdir tmpsub/level1/sublevel1.2
mkdir tmpsub/level2/sublevel1.1
ls tmpsub/level1
rm -r tmpsub
HERE
update

for i in sublevel1.1 sublevel1.2; do
	if ! grep "$i" sub/output.dat > /dev/null; then
		echo "Error: '$i' should be in the output file but isn't:" 1>&2
		cat sub/output.dat 1>&2
		exit 1
	fi
done

eotup
