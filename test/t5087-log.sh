#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Test out the --debug-logging feature.
. ./tup.sh

for i in `seq 0 19`; do
	check_not_exist .tup/log/debug.log.$i
done
for i in `seq 0 19`; do
	update --debug-logging > /dev/null
	check_exist .tup/log/debug.log.$i
done
update --debug-logging > /dev/null
check_not_exist .tup/log/debug.log.20

log_good "Tup update at"
touch foo.txt
update --debug-logging
check_exist .tup/log/debug.log.0
check_exist .tup/log/debug.log.1
log_good "Create.*foo.txt"

touch foo.txt
update --debug-logging
log_good "Update.*foo.txt"

rm foo.txt
update --debug-logging
log_good "Delete.*foo.txt"

eotup
