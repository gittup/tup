#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

# Use include_rules in a variant Tupfile
. ./tup.sh
check_no_windows variant

tmkdir sub
tmkdir sub/sub2
tmkdir configs

cat > Tuprules.tup << HERE
var += y
HERE
cat > sub/sub2/Tuprules.tup << HERE
var += y
HERE
cat > sub/sub2/Tupfile << HERE
include_rules
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
ifeq (\$(var),y y)
: |> touch %o |> bar
endif
ifeq (\$(var),n y)
: |> touch %o |> baz
endif
HERE
echo "CONFIG_DEBUG=y" > configs/debug.config
echo "" > configs/default.config
tup variant configs/*.config
tup touch Tupfile
update

check_exist build-default/sub/sub2/bar
check_not_exist build-default/sub/sub2/foo
check_exist build-debug/sub/sub2/bar
check_exist build-debug/sub/sub2/foo
check_not_exist build-default/sub/sub2/baz
check_not_exist build-debug/sub/sub2/baz

cat > Tuprules.tup << HERE
var += n
HERE
tup touch Tuprules.tup
update

check_not_exist build-default/sub/sub2/bar
check_not_exist build-default/sub/sub2/foo
check_not_exist build-debug/sub/sub2/bar
check_exist build-debug/sub/sub2/foo
check_exist build-default/sub/sub2/baz
check_exist build-debug/sub/sub2/baz

eotup
