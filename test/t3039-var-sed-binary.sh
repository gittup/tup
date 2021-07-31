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

# Test using the --binary flag to tup varsed

. ./tup.sh
check_no_ldpreload varsed
cat > Tupfile << HERE
: foo.txt |> tup varsed --binary %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt
echo "hey sup yo" | diff out.txt -

varsetall FOO=y
update
echo "hey 1 yo" | diff out.txt -

varsetall FOO=n
update
echo "hey 0 yo" | diff out.txt -

eotup
