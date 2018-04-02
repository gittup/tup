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

# Some checks to make sure sticky link changes will only cause a command to be
# re-executed when necessary (specifically, when a sticky link is removed that
# is also a normal link).
. ./tup.sh

# Windows seems to have trouble getting the output from the gcc subprocess
# in update_fail_msg
check_no_windows

tmkdir headers
cat > headers/Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
HERE
cat > Tupfile << HERE
: foreach *.c | headers/*.h |> gcc -c %f -o %o |> %B.o
HERE
echo '#include "headers/foo.h"' > foo.c
tup touch foo.c Tupfile headers/Tupfile
update

check_exist foo.o
tup_sticky_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_object_no_exist headers bar.h

# Secretly remove foo.o
rm foo.o

# Adding a new sticky link shouldn't cause foo.o to be re-created.
cat > headers/Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
: |> echo '#define BAR 3' > %o |> bar.h
HERE
tup touch headers/Tupfile
update --no-scan

check_not_exist foo.o
tup_sticky_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_sticky_exist headers bar.h . 'gcc -c foo.c -o foo.o'

# Removing an unused sticky link shouldn't cause foo.o to be re-created.
cat > headers/Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
HERE
tup touch headers/Tupfile
update --no-scan

check_not_exist foo.o
tup_sticky_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_object_no_exist headers bar.h

# Only removing a sticky link that is used should try to re-compile (and fail)
cat > headers/Tupfile << HERE
HERE
tup touch headers/Tupfile
update_fail_msg "headers/foo.h.*\(No such file or directory\|file not found\)"

# Fix the C file and re-build
echo '' > foo.c
tup touch foo.c
update --no-scan
tup_object_no_exist headers foo.h
tup_object_no_exist headers bar.h

eotup
