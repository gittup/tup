#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Make sure a tup.config not at a top-level doesn't create a new variant.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir build2
tmkdir build2/debug

echo "" > build/tup.config
echo "CONFIG_DEBUG=y" > build2/debug/tup.config

tmkdir sub
cat > sub/Tupfile << HERE
ifeq (@(DEBUG),y)
: foo.c |> cp %f %o |> foo
endif
: foo.c |> cp %f %o |> bar
HERE
tup touch build/tup.config build2/debug/tup.config sub/Tupfile sub/foo.c
update

check_not_exist build2/debug/sub

eotup
