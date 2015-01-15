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

# The monitor changes its working directory to the directory of a file
# modification.  In this test, we get the monitor to have its current directory
# set to a location that is then deleted. A future modification to another
# valid file should allow the autoupdate to work, instead of trying to run it
# from a deleted directory.

. ./tup.sh
check_monitor_supported
monitor --autoupdate

mkdir dir2
cat > dir2/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > dir2/ok.c

tup flush
sym_check dir2/ok.o foo

mkdir tmp
cat > tmp/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > tmp/ok.c
tup flush
sym_check tmp/ok.o foo

# Monitor now has CWD as tmp, so remove that directory.
rm -rf tmp
tup flush

# Make a change to the still existing file - the autoupdate should work
# successfully.
echo 'int bar(void) {return 7;}' > dir2/ok.c
tup flush
sym_check dir2/ok.o bar

eotup
