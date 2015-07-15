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
cat > Tupfile << HERE
: a.file |> cat %f > %o |> %f.processed
: foreach *.file ^a.file ^c.file |> cat %f > %o |> %f.processed
HERE
tup touch a.file b.file c.file d.file
update

tup_object_exist . 'cat a.file > a.file.processed'
tup_object_exist . 'cat b.file > b.file.processed'
tup_object_exist . 'cat d.file > d.file.processed'
tup_object_no_exist . 'cat c.file > c.file.processed'
check_not_exist c.file.processed

eotup
