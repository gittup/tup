#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
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

# Same as t4016, only down in a sub/dir. This just checks some internal details.

. ./tup.sh

tmkdir sub
tmkdir sub/dir
cd sub/dir

tmkdir tmp
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

tup touch bar tmp/bar Tupfile ok.sh
update

tup_dep_exist sub/dir/tmp bar sub/dir './ok.sh'
tup_dep_no_exist sub/dir bar sub/dir './ok.sh'

eotup
