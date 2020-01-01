#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2020  Mike Shal <marfey@gmail.com>
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

# Make sure we work with 'sed -i', which has an unusual temporary file creation
# characteristic - it creates a file with mode 000 so it can't be opened by
# other processes (I guess?). In fuse, if create() is not implemented, it
# mimics it by doing mknod() and then open(). Unfortunately, if the mknod()
# uses mode 000, then the open() fails. Therefore, tup needs to implement
# create() so this can be done in a single call.

. ./tup.sh

# The foo.txt.tmp file is created and then renamed with NtSetInformationFile.
# Trying to hook this currently crashes tup - not sure why.
check_no_windows TODO: Fix NtSetInformationFile hook

# Note we need to explicitly give an extension (-i.tmp) since OSX will create
# a file called 'foo.txt-e' even though it says it won't in the man page.
cat > ok.sh << HERE
echo hey > foo.txt
sed -i.tmp -e 's/hey/there/' foo.txt
HERE
cat > Tupfile << HERE
: |> sh ok.sh |> foo.txt foo.txt.tmp
HERE
tup touch ok.sh Tupfile
update

eotup
