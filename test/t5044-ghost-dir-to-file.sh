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

# Make sure changing a ghost dir to a real file works. Basically the same as
# t5044, only we're checking to see that a file can still have ghost children.

. ./tup.sh
cat > ok.sh << HERE
cat secret/ghost 2>/dev/null || echo nofile
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff - output.txt

touch secret
tup touch secret
update

rm secret
tup rm secret
tmkdir secret
echo boo > secret/ghost
tup touch secret/ghost
update

echo boo | diff - output.txt

eotup
