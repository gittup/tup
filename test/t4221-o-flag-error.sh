#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022-2023  Mike Shal <marfey@gmail.com>
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

# Make sure we can actually see stderr if a command with ^o fails and hasn't
# created the output yet.

. ./tup.sh

cat > ok.sh << HERE
echo "Fail msg" 1>&2
exit 1
HERE
cat > Tupfile << HERE
: |> ^o^ sh ok.sh |> output
HERE
update_fail_msg "Fail msg"

eotup
