#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# Make sure ghost nodes in subdirectories work. I had a small bug where I was
# selecting against (dt, name) and if that failed insertting into (newdt, name).
# I ended up with a bunch of duplicate nodes. Here I check for that by making
# two scripts dependent on the same ghost node, then creating the ghost. Both
# scripts should update.

. ./tup.sh

tmkdir include
cat > ok.sh << HERE
if [ -f include/ghost ]; then cat include/ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
: |> ./foo.sh > %o |> foo-output.txt
HERE
chmod +x ok.sh
cp ok.sh foo.sh
tup touch ok.sh foo.sh Tupfile
update
echo nofile | diff output.txt -
echo nofile | diff foo-output.txt -

echo 'alive' > include/ghost
tup touch include/ghost
update
echo alive | diff output.txt -
echo alive | diff foo-output.txt -

eotup
