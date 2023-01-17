#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Try to use %f in an output variable, similar to $(CFLAGS_%f) in a command
# string.

. ./tup.sh
cat > Tupfile << HERE
files_foo.in += foo1.out
files_foo.in += foo2.out
files_bar.in += bar1.out
: foreach *.in |> echo %f > %o |> \$(files_%f)
HERE
touch foo.in bar.in
parse

tup_dep_exist . 'echo foo.in > foo1.out foo2.out' . foo1.out
tup_dep_exist . 'echo foo.in > foo1.out foo2.out' . foo2.out
tup_dep_exist . 'echo bar.in > bar1.out' . bar1.out

eotup
