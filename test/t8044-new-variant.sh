#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Make sure the build dir for a new variant is not propagated to other
# variants.
. ./tup.sh

mkdir build1
mkdir build2

touch build1/tup.config
update

check_exist build1/build2
check_not_exist build2/build1

touch build2/tup.config
update

check_not_exist build1/build2
check_not_exist build2/build1

rm build1/tup.config
update

check_not_exist build1/build2
check_exist build2/build1

eotup
