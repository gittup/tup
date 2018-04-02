#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# Make sure a new incoming link deletes the old link. This can happen if the
# command gets marked for deletion because we're reparsing the Tupfile, and
# some other command comes in and snipes the output file.

. ./tup.sh
cat > Tupfile << HERE
: |> echo foo > %o |> output
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> echo bar > %o |> output
: |> echo foo > %o |> output
HERE
tup touch Tupfile
update_fail_msg "Unable to create output file 'output'"

eotup
