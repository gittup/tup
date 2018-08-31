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

# Try to ignore an output with the ^o flag, since that goes through different
# updater code.

. ./tup.sh

cat > Tupfile << HERE
: |> ^o^ sh run.sh |> output.txt ^/ignore
HERE
cat > run.sh << HERE
touch output.txt
touch ignore1
touch ignore2
mkdir ignoredir
touch ignoredir/file.txt
HERE
update

check_exist ignore1
check_exist ignore2
check_exist ignoredir/file.txt

eotup
