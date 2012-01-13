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

# Execute a run-script from an included file.
. ./tup.sh
check_no_windows run-script

tmkdir sub
cat > sub/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x sub/gen.sh

cat > sub/inc.tup << HERE
run \$(TUP_CWD)/gen.sh
HERE
cat > Tupfile << HERE
include sub/inc.tup
HERE
tup touch Tupfile sub/inc.tup sub/gen.sh foo.c bar.c
update

check_exist foo.o bar.o

eotup
