#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2016  Mike Shal <marfey@gmail.com>
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

# Make sure we aren't ever root in a subprocess.

. ./tup.sh

cat > Tupfile << HERE
: |> whoami > %o |> output.txt
HERE
update

if echo root | diff - output.txt > /dev/null; then
	echo "Error: 'whoami' shouldn't be root." 1>&2
	exit 1
fi

eotup
