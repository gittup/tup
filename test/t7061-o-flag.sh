#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022  Mike Shal <marfey@gmail.com>
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

# Make sure autoupdates don't get stuck in an endless loop if a rule with ^o
# fails.
. ./tup.sh
check_monitor_supported
set_autoupdate
monitor
cat > Tupfile << HERE
: foreach *.c |> ^o^ gcc -c %f -o %o |> %B.o
HERE

echo "int main(void) {return 0;}" > foo.c
tup flush
check_exist foo.o

# This update will fail, leaving some errors in the test log. The flush should
# return though, without the monitor getting stuck in an endless loop.
echo "int main(void) {return 0;} borf" > foo.c
tup flush

# Quick spot-check to make sure that removing the rule will also remove the
# output file rather than resurrect it.
cat > Tupfile << HERE
HERE
tup flush
check_not_exist foo.o

eotup
