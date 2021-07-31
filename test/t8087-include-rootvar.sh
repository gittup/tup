#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Try to use a _ROOT variable to include a file
. ./tup.sh

mkdir build

cat > Tuprules.tup << HERE
GITTUP_ROOT = \$(TUP_CWD)
HERE

mkdir sub
cat > sub/Tupfile << HERE
include_rules
include \$(GITTUP_ROOT)/lib/lib.tup
: |> echo \$(CFLAGS) |>
HERE

mkdir lib
cat > lib/lib.tup << HERE
CFLAGS += -I\$(TUP_CWD)
CFLAGS += -I\$(TUP_VARIANTDIR)
HERE

touch build/tup.config
update

eotup
