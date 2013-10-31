#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Try to use a python client in a run script and use @-variables

. ./tup.sh
check_no_windows client run-script
check_python

cat > foo.py << HERE
import tup_client
var = tup_client.config_var('FOO')
if var is None:
	print(": |> echo None |>")
else:
	# python 3 is ugly.
	print(" ".join([": |> echo foo", var, "|>"]))
HERE
cat > Tupfile << HERE
run PYTHONPATH=../.. python -B foo.py
HERE
tup touch Tupfile foo.py
update

tup_object_exist . 'echo None'

varsetall FOO=y
update
tup_object_exist . 'echo foo y'

varsetall FOO=hey
update
tup_object_exist . 'echo foo hey'

eotup
