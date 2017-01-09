#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Make sure a ghost node from a previous command doesn't affect a new command
# that doesn't rely on its ghostly past.

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
echo nofile | diff output.txt -
tup_dep_exist . ghost . './ok.sh > output.txt'

# Change ok.sh so it doesn't try to read from ghost, and make sure the
# dependency is gone.
cat > ok.sh << HERE
echo nofile
HERE
tup touch ok.sh
update
tup_object_no_exist . ghost

# Just as a double-check of sorts - actually create the ghost node and update,
# after deleting output.txt from behind tup's back. The output.txt file
# shouldn't be re-created (as it would be in t2028).
echo 'hey' > ghost
tup touch ghost
rm -f output.txt
update --no-scan
check_not_exist output.txt

eotup
