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

# Test the TUP_CWD variable

. ./tup.sh
tmkdir fs
cat > fs/Tupfile << HERE
include ../bar/Install.tup
include ../tab/Install.tup
: foreach \$(lib) |> cp %f %o |> %b
HERE
tmkdir bar
cat > bar/Install.tup << HERE
lib += \$(TUP_CWD)/foo.so
HERE

tmkdir tab
cat > tab/Install.tup << HERE
lib += \$(TUP_CWD)/blah.so
HERE

touch bar/foo.so
touch tab/blah.so

tup touch fs/Tupfile bar/Install.tup tab/Install.tup bar/foo.so tab/blah.so
update
tup_object_exist fs foo.so blah.so
check_exist fs/foo.so fs/blah.so

eotup
