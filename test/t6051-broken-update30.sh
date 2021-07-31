#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# With find_existing_command(), we try to rename a command node to a new name
# if we find that the output already has a command pointing to it (ie:
# we change 'gcc -c foo.c' to 'gcc -W -c foo.c' or something). This would just
# try to set the name on the node. However, if a node named 'gcc -W -c foo.c'
# already existed, then we'd get an SQL error because we would violate the
# unique constraint.
#
# In the test below, we have a './ok.sh' command node, and the touch command
# points to the file 'cwin'. Then we try to change the command that points to
# 'cwin' to be './ok.sh', so the rename fails. That shouldn't happen and instead
# we can just keep using the existing './ok.sh' and update the links
# appropriately.

. ./tup.sh

cat > ok.sh << HERE
cat a
touch b
HERE
chmod +x ok.sh
cat > Tupfile << HERE
: a |> ./ok.sh |> b
: |> touch %o |> cwin
HERE
touch a ok.sh Tupfile
update

cat > ok.sh << HERE
cat a
touch cwin
HERE
cat > Tupfile << HERE
: a |> ./ok.sh |> cwin
HERE
touch Tupfile
update

eotup
