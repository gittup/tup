#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Special input syntax to exclude some specific nodes from, for example,
# foreach scope

. ./tup.sh

echo ': *.file ^c.file |> wc %f > %o |> lines.wc' > Tupfile

tup touch a.file b.file c.file d.file Tupfile
update

echo '0 0 0 a.file
0 0 0 b.file
0 0 0 d.file
0 0 0 total' | diff -u lines.wc -

echo ': *.file ^[bc].file |> wc %f > %o |> lines.wc' > Tupfile

tup touch a.file b.file c.file d.file Tupfile
update

echo '0 0 0 a.file
0 0 0 d.file
0 0 0 total' | diff -u lines.wc -

echo ': *.file ^c.file ^[bc].file |> wc %f > %o |> lines.wc' > Tupfile

tup touch a.file b.file c.file d.file Tupfile
update

echo '0 0 0 a.file
0 0 0 d.file
0 0 0 total' | diff -u lines.wc -

eotup
