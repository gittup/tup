#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2021  Mike Shal <marfey@gmail.com>
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

# Make sure !tup_preserve works from lua

. ./tup.sh

cat > Tupfile.lua << HERE
tup.rule({'file.txt'}, '!tup_preserve')
tup.rule('file.txt', 'sh gen.sh %f %o', 'out.txt')
HERE
mkdir sub
mkdir sub/bar
cp Tupfile.lua sub/bar/Tupfile.lua
cat > gen.sh << HERE
(echo -n "generated "; cat \$1) > \$2
HERE
cp gen.sh sub/bar

echo 'some content' > file.txt
echo 'subdir content' > sub/bar/file.txt

# test that usage without variants don't result in errors
update

echo 'generated some content' | diff - out.txt
echo 'generated subdir content' | diff - sub/bar/out.txt

mkdir build
touch build/tup.config
update

# test that a variant works as well
cmp file.txt build/file.txt
cmp sub/bar/file.txt build/sub/bar/file.txt
echo 'generated some content' | diff - build/out.txt
echo 'generated subdir content' | diff - build/sub/bar/out.txt

# Make sure we can re-parse the Tupfile now that we have file.txt in the srcdir
# and the build dir.
touch Tupfile.lua sub/bar/Tupfile.lua
update > .tup/.tupoutput
if grep 'preserve file.txt' .tup/.tupoutput > /dev/null; then
	cat .tup/.tupoutput
	echo "Error: No preserve commands should run when nothing was changed." 1>&2
	exit 1
fi

# test that the file contents have been preserved
cmp file.txt build/file.txt
cmp sub/bar/file.txt build/sub/bar/file.txt
echo 'generated some content' | diff - build/out.txt
echo 'generated subdir content' | diff - build/sub/bar/out.txt

eotup
