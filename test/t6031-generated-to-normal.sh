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

# Here we try to change a generated file into a normal file in one step. This
# could happen if there's a program to create a file, and then later you decide
# to just check the file into the repo. If someone else pulls your changes
# (which would include the rule removal and file addition), they should be able
# to build in one step.

. ./tup.sh
cat > Tupfile << HERE
: foo.txt |> cp %f %o |> bar.txt
HERE
echo 'orig' > foo.txt
tup touch foo.txt Tupfile
update

check_exist foo.txt bar.txt

# Just try to overwrite bar.txt - should be regenerated with the original text.
echo 'new file' > bar.txt
tup touch bar.txt
update

echo orig | diff - bar.txt

# Now overwrite bar.txt and remove the rule for it. The file should stay put
# with the new text.
echo 'new file' > bar.txt
echo "" > Tupfile
tup touch bar.txt Tupfile
update

check_exist foo.txt bar.txt

echo 'new file' | diff - bar.txt

tup_object_exist . bar.txt

eotup
