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

# sym-cycle: a game about a wheel?
. ./tup.sh
check_no_windows shell

ln -s a b
ln -s b a
cat > Tupfile << HERE
: |> if [ -f a ]; then cat a 2>/dev/null; else echo yo; fi > %o |> output.txt
HERE
update
echo yo | diff - output.txt

cat > Tupfile << HERE
: |> echo yoi > %o |> output.txt
HERE
rm -f a b
update
echo yoi | diff - output.txt
tup_object_no_exist . a b

eotup
