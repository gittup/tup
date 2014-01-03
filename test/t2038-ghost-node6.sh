#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# See if we have a ghost node because it is used in a rule and a symlink points
# to it, then delete the symlink. The ghost should hang around because of its
# use in a rule.

. ./tup.sh
check_no_windows symlink
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
ln -s ghost foo
tup touch foo ok.sh Tupfile
update
echo nofile | diff output.txt -

rm -f foo
tup rm foo
tup_object_exist . ghost

echo 'alive' > ghost
tup touch ghost
update
echo alive | diff output.txt -

eotup
