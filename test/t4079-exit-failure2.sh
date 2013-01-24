#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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
# successfully created, but not those that were created and aren't actually
# outputs.

. ./tup.sh
check_no_windows output_tmp

cat > ok.sh << HERE
echo info > log.txt
echo haha > badfile.txt
exit 2
echo hey > foo.txt
HERE

cat > Tupfile << HERE
tup.definerule{outputs = {'log.txt', 'foo.txt'}, command = 'sh ok.sh'}
HERE
tup touch Tupfile ok.sh
update_fail_msg "File.*badfile.txt.*was written to"

check_exist log.txt
check_not_exist badfile.txt
check_not_exist foo.txt

echo info | diff - log.txt

eotup
