#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Now try changing a variable and see the client re-execute in a variant

. ./tup.sh
check_no_windows client

tmkdir build
touch build/tup.config

make_tup_client
tmkdir sub
cd sub
mv ../client .

cat > Tupfile << HERE
: |> ./client defg > %o |> ok.txt
HERE
tup touch Tupfile empty.txt
update

diff empty.txt ../build/sub/ok.txt

tup_object_exist build/tup.config defg

cd ..
echo "CONFIG_defg=hey" > build/tup.config
tup touch build/tup.config
update

cd sub
echo 'hey' | diff - ../build/sub/ok.txt

eotup
