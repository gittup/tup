#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2015  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup init' works without the --force flag that is used by default
# for these test cases.

. ./tup.sh

tmpdir="/tmp/tup-t0001"
cleanup()
{
	cd /tmp
	rm -rf $tmpdir
}

trap cleanup INT TERM
cleanup
mkdir $tmpdir
cd $tmpdir
tup init
for i in db object shared tri; do
	if [ ! -f ".tup/$i" ]; then
		echo ".tup/$i not created!" 1>&2
		cleanup
		exit 1
	fi
done
cleanup

cd $tupcurdir
eotup
