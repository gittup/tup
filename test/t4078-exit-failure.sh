#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# If the process fails, we should still move over any files that were
# successfully created. This could include a log file, for example.

. ./tup.sh
check_no_windows Unreliable return codes: http://cygwin.1069669.n5.nabble.com/Intermittent-failures-retrieving-process-exit-codes-td94814.html

cat > ok.sh << HERE
echo info > log.txt
exit 2
echo hey > foo.txt
HERE

cat > Tupfile << HERE
: |> sh ok.sh |> log.txt foo.txt
HERE
tup touch Tupfile ok.sh
update_fail_msg "failed with return value 2"

check_exist log.txt
check_not_exist foo.txt

echo info | diff - log.txt

eotup
