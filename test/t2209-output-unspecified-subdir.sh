#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# If a directory doesn't exist and we create a file there, the LD_PRELOAD
# server fails to find the tupid of the parent directory of the file, which
# triggered a different error condition in update_write_info() that didn't
# delete the errant file. This is different in the FUSE server because of the
# directory file mapping which prevents the directory from being created in the
# first place. We should remove the errant file regardless of whether the
# directory was present beforehand or not, otherwise we run the risk of being
# unable to declare it as an output without removing it manually.

. ./tup.sh

cat > ok.sh << HERE
mkdir -p sub/dir
gcc -c foo.c -o foo.o
touch sub/dir/bar
HERE

cat > Tupfile << HERE
: foo.c |> sh ok.sh |> foo.o
HERE
touch foo.c
update_fail_msg "\(File '.*sub.dir.bar' was written to\|Directory.*sub/dir.*Only temporary directories can be created\)"

cat > Tupfile << HERE
: foo.c |> sh ok.sh |> foo.o sub/dir/bar
HERE
update

eotup
