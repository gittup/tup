#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Verify that readdir will correctly read output files. With the mapping
# done in fuse, we need to make sure that readdir contains all the entries
# that will be in the directory.

. ./tup.sh

cat > ok.sh << HERE
echo hey > foo.txt
echo yo > bar.txt
ls *.txt
rm foo.txt bar.txt
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
update

if ! grep 'foo.txt' output.dat > /dev/null; then
	echo "Error: 'foo.txt' should be in the output file" 1>&2
	exit 1
fi
if ! grep 'bar.txt' output.dat > /dev/null; then
	echo "Error: 'bar.txt' should be in the output file" 1>&2
	exit 1
fi

eotup
