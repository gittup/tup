#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2016  Mike Shal <marfey@gmail.com>
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

# Same as t4079, but now the file we accidentally write to is a ghost.

. ./tup.sh

cat > ok.sh << HERE
echo info > log.txt
echo haha > ghost.txt
exit 2
echo hey > foo.txt
HERE

cat > gen-output.sh << HERE
if [ -f ghost.txt ]; then cat ghost.txt; else echo nofile; fi
HERE
chmod +x gen-output.sh

cat > Tupfile << HERE
: |> ./gen-output.sh > %o |> output.txt
: output.txt |> sh ok.sh |> log.txt foo.txt
HERE
tup touch Tupfile ok.sh
update_fail_msg 'Unspecified output'

check_exist log.txt
check_not_exist ghost.txt
check_not_exist foo.txt

echo info | diff - log.txt

eotup
