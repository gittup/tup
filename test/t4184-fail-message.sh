#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2017-2020  Mike Shal <marfey@gmail.com>
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

# Make sure that failing to write output files (because the command fails)
# doesn't mention input file dependencies.
. ./tup.sh
check_no_windows shell

# First check that we don't see any message about "input dependencies"
cat > Tupfile << HERE
: |> false && touch foo.o |> foo.o
HERE
tup touch Tupfile
if __update 2> .tup/.tupoutput; then
	echo "*** Expected update to fail, but didn't" 1>&2
	exit 1
fi

if grep 'failed to process input dependencies' .tup/.tupoutput > /dev/null; then
	echo "Error: The error message shouldn't mention input dependencies" 1>&2
	exit 1
fi

# Check that we get the input dependencies message if they failed and the command failed
cat > Tupfile << HERE
: |> touch %o |> out.txt
: |> if [ -f out.txt ]; then cat out.txt; fi; false |> out2.txt
HERE
tup touch Tupfile
update_fail_msg "failed with return value" "Additionally, the command failed to process input dependencies"

# Check that we get the input dependencies message if they failed and the command succeeded
cat > Tupfile << HERE
: |> touch %o |> out.txt
: |> if [ -f out.txt ]; then cat out.txt; fi; touch %o |> out2.txt
HERE
tup touch Tupfile
update_fail_msg "Command ran successfully, but failed due to errors processing input dependencies."

# Check that we get the output dependencies message if everything worked except outputs
cat > Tupfile << HERE
: |> touch out |> out.txt
HERE
tup touch Tupfile
update_fail_msg "Command failed due to errors processing the output dependencies."

# Check when both inputs & outputs fail
cat > Tupfile << HERE
: |> touch %o |> out.txt
: |> if [ -f out.txt ]; then cat out.txt; fi; touch out2.txt |> out3.txt
HERE
tup touch Tupfile
update_fail_msg "Command ran successfully, but failed due to errors processing both the input and output dependencies"

eotup
