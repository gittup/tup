#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Make sure an external dependency that has a symlink directory component
# works.
. ./tup.sh
check_no_windows symlink
check_tup_suid

# Re-init in a subdir so we can control the external directory contents.
mkdir external
mkdir external/arch-x86
cd external
ln -s arch-x86 arch
cd ..
echo foo1 > external/arch-x86/foo.h
mkdir tmp
cd tmp
re_init
set_full_deps

# Use readlink to get a dependency directly on the symlink, and also read from
# a file using it as a directory.
cat > Tupfile << HERE
: |> readlink ../external/arch; cat ../external/arch/foo.h > %o |> out.txt
HERE
update

echo foo1 | diff - out.txt

update_null "No files should have been recompiled when nothing was changed."

echo foo2 > ../external/arch-x86/foo.h
update

echo foo2 | diff - out.txt

eotup
