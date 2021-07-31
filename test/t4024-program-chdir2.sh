#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Now try to chdir outside of the tup hierarchy and back in.

. ./tup.sh

mkdir tmp
cd tmp
re_init

mkdir sub
cd sub
cat > ok.sh << HERE
#! /bin/sh

cd ../..
cd   -   > /dev/null

cat bar > foo
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh |> foo
HERE

echo "yo" > bar

touch bar Tupfile ok.sh
update

tup_dep_exist sub bar sub './ok.sh'

eotup
