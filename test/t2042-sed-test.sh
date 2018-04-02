#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# See if we can use backslashes in sed without having to escape them.
#
# Note that I have to escape them in the shell here. In reality the Tupfile
# looks like:
#
# : |> echo 'foo$' | sed 's/o\$/3/' > %o |> out.txt
#
# Which matches how we would write it on the command line (the '\$' in the sed
# command is supposed to match the literal '$' in 'foo$').

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: |> echo 'foo\$' | sed 's/o\\\$/3/' > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'fo3' | diff - out.txt

eotup
