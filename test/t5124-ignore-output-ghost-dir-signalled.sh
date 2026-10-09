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

# Interrupted commands also process excluded writes. They must not leave normal
# files beneath ghost directories and prevent the next scan from running.
. ./tup.sh
check_no_windows process

cat > Tupfile << HERE
: |> exec sh run.sh |> stamp ^/temp_files/temporary.txt
HERE
cat > run.sh << HERE
cat temp_files/test 2>/dev/null || echo missing
touch stamp
HERE
update
tup_object_exist . temp_files

cat > Tupfile << 'HERE'
: |> exec sh run.sh |> stamp ^/temp_files(/(?!test$).*)?$
HERE
cat > run.sh << 'HERE'
mkdir -p temp_files
echo ignored > temp_files/temporary.txt
kill -TERM $$
HERE
update_fail_msg 'killed by signal 15'
tup scan

# The interrupted command must remain pending, and a subsequent successful
# update must be able to complete without rebuilding the database.
cat > run.sh << HERE
mkdir -p temp_files
echo ignored > temp_files/temporary.txt
touch stamp
HERE
update
tup scan
update

eotup
