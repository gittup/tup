#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# Make sure we can readdir() outside the .tup hierarchy during parsing, even if
# we are still inside the fuse filesystem.

. ./tup.sh
check_no_windows run-script

touch foo.txt
touch bar.txt

mkdir tmp
cd tmp
re_init

cat > Tupfile << HERE
run sh foo.sh
HERE
cat > foo.sh << HERE
for i in ../*.txt; do
	echo ": |> echo \$i |>"
done
HERE
tup touch foo.sh Tupfile
update

tup_object_exist . 'echo ../foo.txt'
tup_object_exist . 'echo ../bar.txt'

eotup
