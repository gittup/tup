#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Make sure we can't include a generated Tupfile. Since the file would be
# written way after it needs to be parsed, we'd have to do some kind of
# reloading thing like make does, which would be silly.
#
# The generated Tupfile has to be done in a separate directory - if it's done
# in the same directory, then it will be marked deleted before the Tupfile is
# parsed, so that would get an error anyway. This test is to specifically check
# that it should error on the fact that it was a generated file.

. ./tup.sh
tmkdir foo
cat > foo/Tupfile << HERE
: |> echo "var=foo" > %o |> inc
HERE
touch Tupfile

tup touch Tupfile foo/Tupfile
update

cat > Tupfile << HERE
include foo/inc
HERE
tup touch Tupfile
update_fail

eotup
