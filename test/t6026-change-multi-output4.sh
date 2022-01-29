#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# Similar to t6003, only we forget to update the shell script. The command
# should still try to run again, so that we can find out that it is writing to
# the wrong files.

. ./tup.sh
cat > Tupfile << HERE
: |> sh ok.sh |> a b
HERE

cat > ok.sh << HERE
touch a
touch b
HERE

update
check_exist a b
check_not_exist c

cat > Tupfile << HERE
: |> sh ok.sh |> a c
HERE

update_fail

cat > ok.sh << HERE
touch a
touch c
HERE
update

check_exist a c
check_not_exist b

eotup
