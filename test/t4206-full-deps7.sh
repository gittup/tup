#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Make sure we can get an external dependency on a ghost by trying to access a
# file as if it were a directory.
. ./tup.sh
check_tup_suid

# Re-init in a subdir so we can control the external directory contents.
mkdir external
touch external/foo
mkdir tmp
cd tmp
re_init
set_full_deps

cat > Tupfile << HERE
: |> sh run.sh |> out.txt
HERE
cat > run.sh << HERE
if [ -f ../external/foo/bar/baz.txt ]; then cat ../external/foo/bar/baz.txt; else echo nofile; fi > out.txt
HERE
tup touch Tupfile
update

echo "nofile" | diff - out.txt

rm ../external/foo
mkdir ../external/foo
mkdir ../external/foo/bar
echo hey > ../external/foo/bar/baz.txt
update

echo "hey" | diff - out.txt

eotup
