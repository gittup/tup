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

# Make sure if we have a ghost node that it doesn't get used in wildcarding or
# explicit inputs.

. ./tup.sh
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
tup_object_exist . ghost
echo nofile | diff output.txt -

# This should parse correctly because the g* shouldn't match the ghost node
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
: foreach g* |> cat %f |>
HERE
tup touch Tupfile
parse
tup_object_no_exist . 'cat ghost'
update

# When we explicitly name the file, it should fail
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
: ghost |> cat %f |>
HERE
tup touch Tupfile
parse_fail_msg "Explicitly named file 'ghost' is a ghost file, so it can't be used as an input"

eotup
