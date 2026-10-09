#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2026  Mike Shal <marfey@gmail.com>
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

# Regression for 698ff025: removing a variant must recursively clean generated
# directories, not just directories copied from the source tree.
. ./tup.sh

mkdir build
echo CONFIG_BUILD=y > build/tup.config
cat > Tupfile << HERE
ifeq (@(BUILD),y)
: |> echo generated > %o |> generated/nested/output.txt
endif
HERE
update
check_exist build/generated/nested/output.txt
tup_object_exist build/generated nested
tup_object_exist build/generated/nested output.txt

# Leave the physical directories in place so delete_variant_dir() has to recurse
# into them. Removing the entire variant beforehand would miss this regression.
rm build/tup.config
update
check_not_exist build/generated
tup_object_no_exist build generated
tup scan
update

eotup
