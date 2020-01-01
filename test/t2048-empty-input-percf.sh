#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2020  Mike Shal <marfey@gmail.com>
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

# See that using %[fbB] when there is no input gets an error.

. ./tup.sh
cat > Tupfile << HERE
: |> cat %f > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%f used in rule pattern and no input files were specified"

cat > Tupfile << HERE
: |> cat %b > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%b used in rule pattern and no input files were specified"

cat > Tupfile << HERE
: |> cat %B > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%B used in rule pattern and no input files were specified"

cat > Tupfile << HERE
: |> cat %i > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%i used in rule pattern and no order-only input files were specified"

eotup
