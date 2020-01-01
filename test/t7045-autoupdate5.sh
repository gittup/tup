#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# Make sure we do an initial update if autoupdate is requested. Otherwise some
# files may be modified, and you start the monitor, but you still have to
# manually 'tup upd' once or touch a random file to trigger the update.

. ./tup.sh
check_monitor_supported
monitor --autoupdate
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > ok.c

tup flush
sym_check ok.o foo

tup stop
sleep 1
echo 'int bar(void) {return 6;}' > ok.c

monitor --autoupdate

tup flush
sym_check ok.o ^foo bar

eotup
