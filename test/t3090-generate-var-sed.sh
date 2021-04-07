#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021  Mike Shal <marfey@gmail.com>
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

# Try to generate a shell script that uses varsed.

. ./tup.sh

cat > Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
echo "This is an email@address.com" >> foo.txt
varsetall FOO=sup

rm -rf .tup
generate $generate_script_name
./$generate_script_name

(echo "hey sup yo"; echo "This is an email@address.com") | diff out.txt -

eotup
