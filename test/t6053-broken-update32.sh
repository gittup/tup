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

# Make sure a generated file can become a normal file while being used as an
# input.

. ./tup.sh

cat > Tupfile << HERE
: |> echo generated > %o |> genfile.txt
: genfile.txt |> cat %f > %o |> output.txt
HERE
update

echo 'generated' | diff - output.txt

cat > Tupfile << HERE
: genfile.txt |> cat %f > %o |> output.txt
HERE
echo 'manual' > genfile.txt
update

echo 'manual' | diff - output.txt

eotup
