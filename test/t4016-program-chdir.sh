#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Let's see if the wrapper thing will work if we chdir to a new directory, and
# then touch a file back in the original directory. Obviously it doesn't make
# sense to touch a file in the new directory since that would be illegal.

. ./tup.sh

mkdir tmp
cat > ok.sh << HERE
#! /bin/sh
cd tmp
cat bar > ../foo
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: tmp/bar |> ./ok.sh |> foo
HERE

echo "yo" > tmp/bar
echo "not this one" > bar

touch bar tmp/bar Tupfile ok.sh
update

tup_dep_exist tmp bar . './ok.sh'
tup_dep_no_exist . bar . './ok.sh'

eotup
