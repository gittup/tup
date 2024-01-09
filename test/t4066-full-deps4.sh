#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2024  Mike Shal <marfey@gmail.com>
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

# If we try to run with full_deps but are not suid, tup should fail.
. ./tup.sh
check_tup_no_suid

set_full_deps

cat > Tupfile << HERE
: |> echo hey |>
HERE
update_fail_msg "tup error: Sub-processes require running in a chroot for full dependency detection, but this kernel does not support namespacing and tup is not privileged."

eotup
