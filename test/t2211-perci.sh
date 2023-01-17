#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2023  Mike Shal <marfey@gmail.com>
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

# Check %i works for order-only inputs

. ./tup.sh

cat > Tupfile << HERE
: |> echo foo > %o |> foo.txt
: |> echo bar > %o |> bar.txt
: dat.in | foo.txt bar.txt |> cat %f > %o && echo %i >> %o |> %B.out
HERE
echo dat > dat.in
parse

tup_object_exist . 'cat dat.in > dat.out && echo foo.txt bar.txt >> dat.out'

eotup
