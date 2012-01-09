#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Make sure we can't use input from a generated symlink to a directory. This
# would be problematic since we don't know at parse time that an output file
# will necessarily be a symlink to a directory, so we can't figure out which
# files to use as input.

. ./tup.sh
check_no_windows symlink

tmkdir arch-x86
cat > Tupfile << HERE
: |> ln -s arch-x86 %o |> arch
HERE
tup touch Tupfile arch-x86/foo.c
update

cat > Tupfile << HERE
: |> ln -s arch-x86 %o |> arch
: arch/*.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
parse_fail_msg "Unable to read from generated file.*arch"

eotup
