#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2020  Mike Shal <marfey@gmail.com>
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

# It should be ok to explicitly list an external file with full_deps enabled.
. ./tup.sh
check_tup_suid

root=$PWD

# Re-init in a subdir so we can control the external directory contents.
mkdir external
touch external/foo.txt
mkdir tmp
cd tmp
re_init
set_full_deps

cat > run.sh << HERE
if [ -f \$1 ]; then cat \$1; else echo "nofile"; fi > \$2
HERE
cat > Tupfile << HERE
: $root/external/foo.txt |> sh run.sh %f %o |> out.txt
HERE
tup touch Tupfile
update

# An external missing file can also work.
cat > Tupfile << HERE
: $root/external/foo.txt |> sh run.sh %f %o |> out.txt
: $root/external/missing.txt |> sh run.sh %f %o |> out2.txt
HERE
tup touch Tupfile
update

echo nofile | diff - out2.txt

# Make sure we don't have a missing.txt ghost node from creating it in the
# previous Tupfile.
cat > Tupfile << HERE
: $root/external/foo.txt |> sh run.sh %f %o |> out.txt
HERE
tup touch Tupfile
update

if [ "$in_windows" = "1" ]; then
	prefix="c:/cygwin"
else
	prefix=""
fi
tup_object_no_exist $prefix$root/external missing.txt
tup_object_exist $prefix$root/external foo.txt

cat > Tupfile << HERE
HERE
tup touch Tupfile
parse

tup_object_no_exist $prefix$root/external foo.txt

eotup
