#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# Make sure just running a regular ar command will include the proper objects.
# I used to make my ar rules 'rm -f %o; ar ...', but now tup should unlink the
# output file automatically before running the command.
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar crs %o %f |> lib.a
HERE
echo "void foo(void) {}" > foo.c
echo "void bar(void) {}" > bar.c
update
sym_check lib.a foo bar

rm foo.c
update
sym_check lib.a ^foo bar

# This is a quick check for commit 794c3ae9ac7bcffe6b6418e4daf03f5c844e1f3a
# to make sure OSX doesn't create .fuse_hidden files from 'ar'.
if ls -la | grep .fuse_hidden > /dev/null; then
	echo "Error: .fuse_hidden file found!" 1>&2
	exit 1
fi

eotup
