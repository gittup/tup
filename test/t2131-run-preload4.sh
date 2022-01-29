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

# Make sure preload gets eval'd.
. ./tup.sh
check_no_windows run-script

mkdir sub
mkdir sub/sub2
touch sub/foo

cat > Tupfile << HERE
subdir = sub
preload \$(subdir)
run sh -e ok.sh
HERE

cat > ok.sh << HERE
for i in sub/*; do
	echo ": |> echo \$i |>"
done
HERE
update

tup_object_exist . 'echo sub/sub2'
tup_object_exist . 'echo sub/foo'

eotup
