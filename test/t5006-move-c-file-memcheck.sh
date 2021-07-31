#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2021  Mike Shal <marfey@gmail.com>
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

# This is basically t5003, but after creating a new file and deleting an old
# file we run with a memory checker. I tried to get an example that exercises
# a fair bit of the usual functionality in a single trace.

echo "[33mTODO: use valgrind? mtrace doesn't work with threads[0m"
exit 0
. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cru %o %f |> libfoo.a
HERE

# Verify both files are compiled
echo "int foo(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
update
sym_check foo.o foo
sym_check bar.o bar1
sym_check libfoo.a foo bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
MALLOC_TRACE=mout tup upd

# Still seem to be some leaks in sqlite, even though I'm finalizing the
# statements and doing an sqlite3_close(). Maybe I'm missing something.
cat mout | grep -v libsqlite3.so | mtrace `which tup` -

eotup
