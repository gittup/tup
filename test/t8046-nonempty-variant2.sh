#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2022  Mike Shal <marfey@gmail.com>
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

# Create a normal file inside a variant directory after the variant is used.
. ./tup.sh

mkdir build
mkdir sub

cat > sub/Tupfile << HERE
: |> touch %o |> generated
HERE
touch build/tup.config
update

touch build/sub/normal
update

rm build/tup.config
update_fail_msg "Unable to clean out variant directory"

eotup
