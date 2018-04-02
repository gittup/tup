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

# Make sure a var/sed command doesn't get re-invoked when the monitor is
# stopped and restarted.

. ./tup.sh
check_no_ldpreload varsed
check_monitor_supported
monitor

cat > Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
: out.txt |> cat %f > %o |> new.txt
HERE
echo "hey @FOO@ yo" > foo.txt
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt new.txt
(echo "hey sup yo") | diff out.txt -
(echo "hey sup yo") | diff new.txt -

echo "a @FOO@ b" > foo.txt
update
(echo "a sup b") | diff out.txt -
(echo "a sup b") | diff new.txt -

stop_monitor
monitor
stop_monitor
check_empty_tupdirs

eotup
