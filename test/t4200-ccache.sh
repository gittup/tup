#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# Make sure we can play nicely with ccache.
. ./tup.sh
min_version="4.7"
version=$( (echo $min_version; ccache --version | grep 'ccache version' | sed 's/ccache version //') | sort -V | head -1)
if [ "$version" != "$min_version" ]; then
	echo "[33mSkipping test: Expected ccache version >= $min_version[0m"
	eotup
fi
check_tup_suid

set_full_deps

cat > Tupfile << HERE
: foreach *.c |> ccache gcc -c %f -o %o |> %B.o
HERE
touch foo.c bar.c
update

echo 'int x;' > foo.c
touch bar.c
update

eotup
