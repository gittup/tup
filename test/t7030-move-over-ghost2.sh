#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2016  Mike Shal <marfey@gmail.com>
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

# We have a ghost Tuprules.tup node, and then move a file over it. The
# necessary directories should be re-parsed.
. ./tup.sh
check_monitor_supported
monitor
mkdir a
mkdir a/a2
cat > a/a2/Tupfile << HERE
include_rules
: |> echo \$(VAR) |>
HERE
echo 'VAR=3' > Tuprules.tup
echo 'VAR=4' > a/ok.txt
update

tup_dep_exist a Tuprules.tup a a2
tup_object_exist a/a2 'echo 3'

mv a/ok.txt a/Tuprules.tup
update

tup_dep_exist a Tuprules.tup a a2
tup_object_exist a/a2 'echo 4'

eotup
