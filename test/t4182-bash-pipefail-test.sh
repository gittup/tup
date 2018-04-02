#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2018  Mike Shal <marfey@gmail.com>
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

# Test that a tuprule with a pipefail fails in bash
. ./tup.sh
check_bash

# Test that normal pipefail passes successfully
cat > Tupfile << HERE
: |> false | true |>
HERE
tup touch Tupfile
update

# Test that if fails in a bash rule
cat > Tupfile << HERE
: |> ^b^ false | true |>
HERE
tup touch Tupfile
update_fail_msg 'Command ID=[0-9]* failed with return value 1'

eotup
