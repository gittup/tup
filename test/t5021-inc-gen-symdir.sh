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

# Make sure we can't include a Tupfile by using a generated symlink somewhere
# else in the path. Here we'll make a directory symlink in a rule, and then
# later try to include a Tupfile by using that directory link.

. ./tup.sh
check_no_windows symlink

mkdir foo
mkdir foo/arch-x86
echo 'var = 3' > foo/arch-x86/rules.tup
cat > foo/Tupfile << HERE
: arch-x86 |> ln -s %f %o |> arch
HERE
update

cat > Tupfile << HERE
include foo/arch/rules.tup
HERE
update_fail

eotup
