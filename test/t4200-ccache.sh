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

# Make sure we can play nicely with ccache.
. ./tup.sh
if ! ccache --version | grep 'ccache version 3' > /dev/null; then
	echo "[33mSkipping test: Expected ccache version 3[0m"
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
