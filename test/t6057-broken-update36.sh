#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
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

# As reported on the mailing list - running find with two different times
# results in the files getting picked up as input dependencies. Before the fix,
# the first update would result in the b.out script having an input dependency
# on a.out (even though the 'Missing input dependency' message was displayed,
# the link was still added). After removing a.out, a.out script is then
# re-scheduled for execution. During the second update, the a.out script runs
# and sees the b.out file, so b.out is now also a dependency on the a.out
# script. This is also detected and reported as an error, but the link is still
# added. Now we have a situation where the a.out script depends on b.out, and
# the b.out script depends on a.out - so the third update detects a circular
# dependency. This obviously should not be possible.

. ./tup.sh

# Can't run in parallel, otherwise both scripts will miss each other.
single_threaded
cat > Tupfile << HERE
: |> ./do_stuff.sh a.out |> a.out
: |> ./do_stuff.sh b.out |> b.out
HERE
cat > do_stuff.sh << HERE
find . -type f | xargs cat > /dev/null
touch \$1
HERE
chmod +x do_stuff.sh
tup touch Tupfile do_stuff.sh
update_fail_msg "Missing input dependency"

tup_dep_no_exist . a.out . './do_stuff.sh b.out'
tup_dep_no_exist . b.out . './do_stuff.sh a.out'

rm a.out
update_fail_msg "Missing input dependency"

tup_dep_no_exist . a.out . './do_stuff.sh b.out'
tup_dep_no_exist . b.out . './do_stuff.sh a.out'

# The third update should result in the same error message, not a circular
# dependency error.
update_fail_msg "Missing input dependency"

eotup
