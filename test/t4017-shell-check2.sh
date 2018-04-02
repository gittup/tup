#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# More canonicalization issues. Create a file called 'bar.sh', then read
# './bar.sh', then unlink it. This apparently worked before when I was
# canonicalizing everything, but now it doesn't.

. ./tup.sh
cat > Tupfile << HERE
: |> ./foo.sh |>
HERE

cat > foo.sh << HERE
echo 'echo hey' > bar.sh
chmod +x bar.sh
./bar.sh
rm bar.sh
HERE
chmod +x foo.sh
tup touch Tupfile foo.sh
update

eotup
