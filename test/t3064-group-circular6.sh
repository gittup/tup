#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2015  Mike Shal <marfey@gmail.com>
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

# Make sure group links are cleaned up when necessary. This is tricky since
# there is no guaranteed order between which of sub1 and sub2 will be parsed
# first, so we have to defer circular dependency checking among groups until
# after all files are parsed and old commands are deleted.
. ./tup.sh

tmkdir sub1
tmkdir sub2

cat > sub1/Tupfile << HERE
: <foo> |> echo blah > %o |> output.txt | <bar>
HERE
update

cat > sub1/Tupfile << HERE
HERE
cat > sub2/Tupfile << HERE
: <bar> |> echo blah > %o |> output.txt | <foo>
HERE
tup touch sub1/Tupfile sub2/Tupfile
update

eotup
