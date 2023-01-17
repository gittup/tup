#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Similar to t8029, but now we have 'bar' as a ghost and then overwrite it
# with a generated file, which is allowed.
. ./tup.sh
check_no_windows shell

mkdir build-default

cat > Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> if [ -f bar ]; then cat bar; else echo nofile; fi > %o |> output.txt
HERE
echo "" > build-default/tup.config
update

echo nofile | diff - build-default/output.txt
tup_object_exist . bar

cat > Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> echo foo > %o |> bar
: bar |> if [ -f %f ]; then cat %f; else echo nofile; fi > %o |> output.txt
HERE
update

echo foo | diff - build-default/output.txt

eotup
