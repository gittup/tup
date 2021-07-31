#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Try to generate a ghost symlink in a subdir from a rule.

. ./tup.sh
check_no_windows symlink
cat > Tupfile << HERE
: |> ln -s secret/ghost %o |> foo
: foo |> cat %f 2>/dev/null || true |>
HERE
update
tup_object_exist . foo secret

rm -f Tupfile
update
tup_object_no_exist secret ghost
tup_object_no_exist . foo secret

eotup
