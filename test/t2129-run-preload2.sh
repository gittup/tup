#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Try a preload from an included script. It should preload from that directory.
. ./tup.sh
check_no_windows run-script

mkdir sub
cat > sub/include.tup << HERE
preload foo
HERE

mkdir sub/foo
touch sub/foo/bar.c

cat > Tupfile << HERE
include sub/include.tup
run sh -e ok.sh
HERE

cat > ok.sh << HERE
for i in sub/foo/*.c; do
	echo ": |> echo \$i |>"
done
HERE
update

tup_object_exist . 'echo sub/foo/bar.c'

eotup
