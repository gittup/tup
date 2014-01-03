#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2014  Mike Shal <marfey@gmail.com>
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

# Make sure the output in another dir is deleted from the db when we remove
# the rule.

. ./tup.sh

tmkdir foo
cat > Tupfile << HERE
: |> touch %o |> foo/ok.txt
HERE
tup touch Tupfile
update

tup_object_exist foo ok.txt
check_exist foo/ok.txt

cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_object_no_exist foo ok.txt
check_not_exist foo/ok.txt

eotup
