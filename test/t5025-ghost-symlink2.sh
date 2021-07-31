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

# See what happens if we have a valid symlink, then remove the destination
# node, and then re-create the destination.

. ./tup.sh
check_no_windows shell
echo "#define FOO 3" > foo-x86.h
ln -s foo-x86.h foo.h
cat > Tupfile << HERE
: foo.h |> (cat %f 2>/dev/null || echo 'nofile') > %o |> output.txt
HERE
update
echo '#define FOO 3' | diff - output.txt
check_updates foo.h output.txt
check_updates foo-x86.h output.txt

rm -f foo-x86.h
update
echo 'nofile' | diff - output.txt
# Careful: Can't do check_updates with foo.h here since the touch() will end
# up changing the sym field of foo.h

echo "#define FOO new" > foo-x86.h
update
echo '#define FOO new' | diff - output.txt
check_updates foo.h output.txt
check_updates foo-x86.h output.txt

eotup
