#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Make sure errors appear under the parser banner.

. ./tup.sh
tmkdir foo
cat > foo/Tupfile.lua << HERE
print "FOO"
HERE
tmkdir bar
cat > bar/Tupfile.lua << HERE
-- Make sure foo is parsed first
tup.rule({'../foo/foo.txt'}, 'echo hey')
print "BAR"
HERE
tup touch foo/foo.txt
tup parse > .output.txt 2>&1 || true
if ! cat .output.txt | tr '\n' ' ' | grep 'foo.*FOO.*bar.*BAR' > /dev/null; then
	cat .output.txt
	echo "Error: Expected printed strings to be under the parser banner." 1>&2
	exit 1
fi

eotup
