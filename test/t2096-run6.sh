#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Make sure only outputs before the run-script is executed are seen
# by readdir(). Otherwise, both the fuse thread and the parser thread
# could be accessing the database at the same time.
. ./tup.sh
check_no_windows run-script

cat > Tupfile << HERE
run sh -e ok.sh
HERE
cat > ok.sh << HERE
echo ": |> touch %o |> foo.c"
for i in *.c; do
	echo ": \$i|> gcc -c %f -o %o |> %B.o"
done
HERE
tup touch Tupfile ok.sh bar.c
update

check_exist bar.o
check_exist foo.c
check_not_exist foo.o

eotup
