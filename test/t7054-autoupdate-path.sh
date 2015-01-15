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

# Similar to t7053, but with autoupdate. Here we always update with the
# cached environment, so if we want to change the environment we have
# to modify an environment and then run 'tup upd'. Any future autoupdates
# will then use those settings.

. ./tup.sh
check_monitor_supported

monitor --autoupdate

cat > run.sh << HERE
echo "var:\$MARF"
HERE

cat > Tupfile << HERE
export MARF
: |> sh run.sh > %o |> out.txt
HERE

tup flush
echo 'var:' | diff - out.txt

export MARF=newvar
touch run.sh
tup flush
echo 'var:' | diff - out.txt

# Run update manually to get the new environment.
tup upd
echo 'var:newvar' | diff - out.txt

# Next autoupdate should keep the new environment.
touch run.sh
tup flush
echo 'var:newvar' | diff - out.txt

eotup
