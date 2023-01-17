#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2023  Mike Shal <marfey@gmail.com>
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

# When we explicitly list the input files (without foreach), the behavior in
# the parser is a bit different. Specifically, the output file isn't put into
# the database until later in the parsing, after the rule that needs that
# output has done a select on it. This addresses a specific bug discovered
# while tupifying sysvinit.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o
: foo.o |> gcc %f -o %o |> prog.exe
HERE

echo "int main(void) {}" > foo.c
update
sym_check foo.o main
tup_object_exist . foo.o prog.exe

# Run a second time, since in theory this time foo.o is in the database, but
# will be moved to DELETE before the Tupfile is re-parsed. So, it's slightly
# different in this case.
touch foo.c Tupfile
update
sym_check foo.o main
tup_object_exist . foo.o prog.exe

eotup
