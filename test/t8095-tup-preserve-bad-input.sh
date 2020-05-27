#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2020  Mike Shal <marfey@gmail.com>
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

# Make sure !tup_preserve fails if given the output of another command.

. ./tup.sh
check_no_windows variant

tmkdir sub
cat > sub/Tupfile << HERE
: |> touch %o |> foo.txt
HERE
echo 'bar' > sub/bar.txt
mkdir build
touch build/tup.config

update

cat > sub/Tupfile << HERE
: |> touch %o |> foo.txt
: foo.txt |> !tup_preserve |>
HERE
tup touch sub/Tupfile

# TODO: This should probably fail with a more explicit message that
# !tup_preserve can't use a generate file, since foo.txt is only a ghost file
# because of how fuse accesses variants. This will probably need to be updated
# with a better message if explicit variants are enabled.
update_fail_msg 'Explicitly.*foo.txt.*is a ghost file'

cat > sub/Tupfile << HERE
: |> touch %o |> foo.txt
: bar.txt |> !tup_preserve |>
HERE
tup touch sub/Tupfile
update

cmp sub/bar.txt build/sub/bar.txt

eotup
