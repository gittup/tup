#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018  Mike Shal <marfey@gmail.com>
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

# Make sure we can exclude an output file when it overwrites an existing file.

. ./tup.sh

cat > Tupfile << HERE
# The first rule is a dummy rule for helgrind to make sure we aren't accessing
# entry structures in process_output() and from fuse at the same time. IOW: We
# can't use tup_entry for exclusions because fuse needs to use them, and we
# can't lock the db from fuse.
: |> sh tmp.sh |>
: |> sh run.sh |> output.txt ^exclude.txt
HERE
cat > tmp.sh << HERE
if [ -f foo ]; then echo foo; else echo bar; fi
HERE
cat > run.sh << HERE
touch output.txt
echo bar > exclude.txt
HERE
echo 'foo' > exclude.txt
update

echo 'bar' | diff - exclude.txt

eotup
