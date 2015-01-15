#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# Slightly more complex ghost dir - see if we have a ghost node (ghost) with a
# dir pointing to another ghost node (secret). Then we make secret a file, and
# then we remove secret and make it a directory, and then create ghost in it.
# Make sure that we don't lose ghost dependencies and that all works somehow.

. ./tup.sh
cat > ok.sh << HERE
cat secret/ghost 2>/dev/null || echo nofile
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
echo nofile | diff output.txt -
tup_dep_exist . secret . './ok.sh > output.txt'

# Create 'secret' as a file - this will cause the command to run
echo 'foo' > secret
tup touch secret
update --no-scan
tup_object_exist . secret
tup_dep_exist . secret . './ok.sh > output.txt'

# Delete the file
rm -f secret
tup rm secret
update --no-scan
tup_dep_exist . secret . './ok.sh > output.txt'

# Once the dir exists we should get a dependency on 'ghost'
tmkdir secret
update --no-scan
tup_dep_exist secret ghost . './ok.sh > output.txt'

# Now we finally re-create ghost. The command should execute at this point.
echo 'alive' > secret/ghost
tup touch secret/ghost
update
echo alive | diff output.txt -

eotup
