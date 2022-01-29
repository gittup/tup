#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2022  Mike Shal <marfey@gmail.com>
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

# Try to use a python client.

. ./tup.sh
check_no_windows client
check_python

mkdir sub

cat > sub/foo.py << HERE
import tup_client
if tup_client.config_var('FOO') is not None:
	raise Exception("No: FOO")
if tup_client.config_var('BAR') is not None:
	raise Exception("No: BAR")
if tup_client.config_var('BAZ') is not None:
	raise Exception("No: BAZ")
HERE
cat > sub/Tupfile << HERE
: |> PYTHONPATH=../../.. python -B foo.py |>
HERE
update

tup_object_exist tup.config BAZ

varsetall FOO=y BAR=hey
update_fail_msg 'Exception: No: FOO'

cat > sub/foo.py << HERE
import tup_client
if tup_client.config_var('FOO') != "y":
	raise Exception("No: FOO")
if tup_client.config_var('BAR') != "hey":
	raise Exception("No: BAR")
if tup_client.config_var('BAZ') is not None:
	raise Exception("No: BAZ")
HERE
update

tup_object_exist tup.config BAZ

cat > sub/foo.py << HERE
import tup_client
if tup_client.config_var('FOO') != "y":
	raise Exception("No: FOO")
if tup_client.config_var('BAR') != "hey":
	raise Exception("No: BAR")
HERE
update

tup_object_no_exist tup.config BAZ

eotup
