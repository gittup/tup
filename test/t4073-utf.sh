#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2016  Mike Shal <marfey@gmail.com>
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

# Make sure we can read and write a utf-8 file.

. ./tup.sh
case $tupos in
Darwin)
	# In OSX, it uses UTF-8 but in a different style. Eg it uses:
	# insert[föo]: 66 6f cc 88 6f
	#
	# But the Tupfile reads as:
	# Name[föo]: 66 c3 b6 6f
	echo "[33mTODO: Unicode currently broken on this platform.[0m" 1>&2
	eotup
	;;
esac

cat > Tupfile << HERE
: föo |> cat %f > %o |> bär
HERE
echo "some text" > föo
update

echo "some text" | diff - bär

eotup
