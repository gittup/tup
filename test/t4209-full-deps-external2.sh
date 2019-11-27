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

# Make sure we don't get spurious ghosts with external files as inputs
. ./tup.sh
check_tup_suid

root=$PWD

# Re-init in a subdir so we can control the external directory contents.
mkdir external
touch external/foo.txt
mkdir tmp
cd tmp
re_init
set_full_deps

cat > Tupfile << HERE
: $root/external/foo.txt |> cat blah.txt > %o |> out.txt
HERE
tup touch Tupfile blah.txt
update

cat > Tupfile << HERE
: |> cat blah.txt > %o |> out.txt
HERE
tup touch Tupfile
update

tup_object_no_exist $root/external foo.txt

eotup
