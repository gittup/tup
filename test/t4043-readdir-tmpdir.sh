#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2016  Mike Shal <marfey@gmail.com>
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

# Same as t4041/t4042, but in a subdirectory with tmp directories.

. ./tup.sh

tmkdir sub
tmkdir sub/dir2
touch sub/dir2/a1.txt
touch sub/dir2/a2.txt
cd sub
cat > ok.sh << HERE
mkdir tmpsub
echo sup > tmpsub/baz.txt
echo hey > foo.txt
echo yo > bar.txt
ls *.txt dir2/*.txt tmpsub/*.txt
rm foo.txt bar.txt
rm tmpsub/baz.txt
rmdir tmpsub
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
cd ..
update

for i in foo.txt bar.txt tmpsub/baz.txt dir2/a1.txt dir2/a2.txt; do
	if ! grep "$1" sub/output.dat > /dev/null; then
		echo "Error: '$1' should be in the output file" 1>&2
		exit 1
	fi
done

eotup
