#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2015  Mike Shal <marfey@gmail.com>
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

# Try extra-outputs, which are like order-only pre-requisites.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: |> echo blah > %o; touch bar |> foo.h | bar
HERE

tup touch Tupfile
parse
tup_dep_exist . "echo blah > foo.h; touch bar" . foo.h
tup_dep_exist . "echo blah > foo.h; touch bar" . bar
update

cat > Tupfile << HERE
: |> echo blah > %o; touch bar |> foo.h
HERE
tup touch Tupfile
parse

tup_dep_exist . "echo blah > foo.h; touch bar" . foo.h
tup_object_no_exist . bar
update_fail_msg "File '.*bar' was written to"

cat > Tupfile << HERE
: |> echo blah > %o; touch bar |> foo.h | bar
HERE
tup touch Tupfile
update

tup_dep_exist . "echo blah > foo.h; touch bar" . foo.h
tup_dep_exist . "echo blah > foo.h; touch bar" . bar

eotup
