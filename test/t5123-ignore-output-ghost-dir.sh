#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2026  Mike Shal <marfey@gmail.com>
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

# An excluded write must convert existing ghost ancestors to normal directories,
# just as it does when those directory entries do not already exist.
. ./tup.sh

cat > Tupfile << HERE
: |> sh run.sh |> stamp ^/temp_files/temporary.txt
HERE
cat > run.sh << HERE
cat temp_files/test 2>/dev/null || echo missing
touch stamp
HERE
update
tup_object_exist . temp_files

cat > Tupfile << 'HERE'
: |> sh run.sh |> stamp ^/temp_files(/(?!.*test$).*)?$
HERE
cat > run.sh << HERE
mkdir -p temp_files/nested
echo ignored > temp_files/temporary.txt
echo ignored > temp_files/nested/temporary.txt
touch stamp
HERE
update
check_exist temp_files/temporary.txt
check_exist temp_files/nested/temporary.txt

# The next scan used to load the normal file without its ghost parent and fail
# with "Unable to find parent entry". Check repeated scans and deletion too.
tup scan
update
rm -rf temp_files
tup scan
update
tup_object_no_exist . temp_files

eotup
