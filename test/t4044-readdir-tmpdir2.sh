#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Same as t4043, only two levels of tmp subdirectories.

. ./tup.sh

mkdir sub
cd sub
cat > ok.sh << HERE
mkdir tmpsub
mkdir tmpsub/level2
echo sup > tmpsub/level2/baz.txt
echo hey > foo.txt
echo yo > bar.txt
# Note: tmpsub/*.txt should not match tmpsub/level2/*.txt
ls *.txt tmpsub/*.txt 2>/dev/null
rm foo.txt bar.txt
rm tmpsub/level2/baz.txt
rmdir tmpsub/level2
rmdir tmpsub
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
cd ..
update

for i in foo.txt bar.txt; do
	if ! grep "$i" sub/output.dat > /dev/null; then
		echo "Error: '$i' should be in the output file" 1>&2
		exit 1
	fi
done
if grep tmpsub/level2/baz.txt sub/output.dat > /dev/null; then
	echo "Error: tmpsub/level2/baz.txt should not be in the output file yet" 1>&2
	exit 1
fi

cat > sub/ok.sh << HERE
mkdir tmpsub
mkdir tmpsub/level2
echo sup > tmpsub/level2/baz.txt
echo hey > foo.txt
echo yo > bar.txt
# Now we use tmpsub/level2/*.txt
ls *.txt tmpsub/level2/*.txt
rm foo.txt bar.txt
rm tmpsub/level2/baz.txt
rmdir tmpsub/level2
rmdir tmpsub
HERE
update

for i in foo.txt bar.txt tmpsub/level2/baz.txt; do
	if ! grep "$i" sub/output.dat > /dev/null; then
		echo "Error: '$i' should be in the output file but isn't:" 1>&2
		cat sub/output.dat
		exit 1
	fi
done

eotup
