#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# Same as t4037, but in a subdirectory.

. ./tup.sh

tmkdir sub
cat > sub/foo.c << HERE
int main(void)
{
	return 0;
}
HERE
cat > sub/bar.c << HERE
void bar(void) {}
HERE
cat > sub/Tupfile << HERE
# Similar to t4037 - add specific flags for OSX
ifeq (@(TUP_PLATFORM),macosx)
stripflags = -u -r
endif
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f && strip \$(stripflags) %o |> libfoo.a
HERE
tup touch sub/foo.c sub/bar.c sub/Tupfile
update

eotup
