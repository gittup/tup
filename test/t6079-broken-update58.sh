#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# If we have a normal file in the modify list, and then run a partial update on
# a command that will use the file, the normal file was getting pruned from the
# graph, so its mtime didn't update. After the command executes, it adds the
# link to the normal file, and is now out of date on the next update because we
# see that the input file has been modified.
#
# Instead tup needs to avoid pruning normal files, so the mtimes can be
# updated. Any other dependent commands are already put in the modify list
# directly, so we don't need the file in the modify list to handle that.
. ./tup.sh

cat > Tupfile << HERE
: |> sh ok.sh |> output.txt
HERE
cat > ok.sh << HERE
touch output.txt
HERE
touch foo.txt
update

cat > ok.sh << HERE
cat foo.txt && touch output.txt
HERE
touch foo.txt
update_partial output.txt

update_partial output.txt > .tup/.output.txt

if grep 'sh ok.sh' .tup/.output.txt > /dev/null; then
	cat .tup/.output.txt
	echo "Error: should not have run ok.sh in partial update." 1>&2
	exit 1
fi

eotup
