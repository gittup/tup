#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2015  Mike Shal <marfey@gmail.com>
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

# Same as t4079, but now the file we accidentally write to already exists.

. ./tup.sh
check_no_windows output_tmp

cat > ok.sh << HERE
echo info > log.txt
echo haha > badfile.txt
exit 2
echo hey > foo.txt
HERE

echo goodtext > badfile.txt

cat > Tupfile << HERE
: |> sh ok.sh |> log.txt foo.txt
HERE
tup touch Tupfile ok.sh badfile.txt
update_fail_msg 'Unspecified output: badfile.txt'

check_exist log.txt
check_exist badfile.txt
check_not_exist foo.txt

echo info | diff - log.txt
echo goodtext | diff - badfile.txt

eotup
