#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Like t8014, but this time we don't involve the parser. Just try to read
# straight from the other variant.
. ./tup.sh
check_no_windows variant

tmkdir build
cat > Tupfile << HERE
: |> touch %o |> foo
HERE
tup touch Tupfile build/tup.config
update

cat > Tupfile << HERE
: |> touch %o |> foo
ifeq (@(FOO),y)
: |> cat ../build/foo > %o |> output
endif
HERE
tup touch Tupfile
update

tmkdir build2
echo "CONFIG_FOO=y" > build2/tup.config
tup touch build2/tup.config
update_fail_msg "Unable to use files from another variant.*in this variant"

eotup
