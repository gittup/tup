#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# If we create an output file in the database, and then update its mtime
# outside of tup, tup will think that the file is going to be resurrected and
# mark it as a valid input to support t6053. This bypasses the check where a
# file is scheduled to be deleted, which is normally how tup catches a file
# being used as both an input and an output. Since the check is skipped, we end
# up with both the input and output link in the database, which resulted in a
# circular dependency error. Now there is a new check to make sure inputs and
# outputs are unique, which fixes this issue and can function as a failsafe to
# prevent similar future issues.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> output.txt
HERE
tup parse
echo ohai > output.txt

cat > Tupfile << HERE
: output.txt |> touch %o |> output.txt
HERE
update_fail_msg "tup error.*lists this file as both an input and an output: output.txt"

eotup
