#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# Make sure we can get a ghost if a file is used as a directory (so we get
# ENOTDIR as the error code).

. ./tup.sh
cat > ok.sh << HERE
cat secret/ghost 2>/dev/null || echo nofile
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
touch secret
update
echo nofile | diff - output.txt

rm secret
mkdir secret
echo 'boo' > secret/ghost
update

echo boo | diff - output.txt

eotup
