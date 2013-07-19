#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Like t4104, but now we have multiple outputs, so find_existing_command
# and tup_db_create_unique_link don't get called on the same files.

. ./tup.sh

tmkdir foo
tmkdir bar
tmkdir baz
echo foo > foo/foo.txt
echo bar > bar/bar.txt
echo baz > baz/baz.txt
cat > foo/Tupfile << HERE
HERE
cat > bar/Tupfile << HERE
: |> cp bar.txt %o |> barout.txt
HERE
cat > baz/Tupfile << HERE
: |> cp baz.txt %o |> bazout.txt
HERE
tup touch foo/Tupfile bar/Tupfile baz/Tupfile
update

echo bar | diff - bar/barout.txt
echo baz | diff - baz/bazout.txt

# Switch bar&baz to foo - this should work.
cat > foo/ok.sh << HERE
cp foo.txt ../bar/barout.txt
cp foo.txt ../baz/bazout.txt
HERE
cat > foo/Tupfile << HERE
: |> sh ok.sh |> ../bar/barout.txt ../baz/bazout.txt
HERE
tup touch foo/Tupfile
rm bar/Tupfile baz/Tupfile
update

eotup
