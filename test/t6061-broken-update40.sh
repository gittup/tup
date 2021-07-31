#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# While looking through the code, I noticed that there was no longer anything
# that actually tested a ghost being in a ghost directory (since fuse doesn't
# try to read from 'secret/ghost' like the ldpreload method did - it just
# stops at 'secret/'). Also it seems that instead of converting a directory
# with a ghost into a ghost itself, it was forcefully removing the child node,
# so dependencies would then be missing. This verifies that behavior.

. ./tup.sh
check_no_windows shell

mkdir secret

cat > Tupfile << HERE
: |> (cat secret/ghost 2>/dev/null || echo nofile) > %o |> output.txt
HERE
touch Tupfile
update
echo 'nofile' | diff - output.txt

rmdir secret
update
echo 'nofile' | diff - output.txt

mkdir secret
echo 'foo' > secret/ghost
update
echo 'foo' | diff - output.txt

eotup
