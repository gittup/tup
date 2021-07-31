#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021  Mike Shal <marfey@gmail.com>
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

# Make sure expanding a group in a variant only includes files in that variant.

. ./tup.sh

mkdir sub
mkdir sub2
mkdir sub3
cat > sub/Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B ../<foo-autoh>
HERE
cat > sub2/Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B ../<foo-autoh>
HERE
cat > sub3/Tupfile << HERE
: ../<foo-autoh> |> echo %<foo-autoh> > %o |> list.txt
HERE
echo '#define FOO 3' > sub/foo.h.in
echo '#define BAR 4' > sub2/bar.h.in
echo '#define BAZ 5' > sub2/baz.h.in
mkdir build
mkdir build2
touch build/tup.config
touch build2/tup.config
update

# Windows has an extra space and \r at the end, so ignore trailing whitespace (\s*)
if ! grep '^[^ ]* [^ ]* [^ ]*\s*$' build/sub3/list.txt > /dev/null; then
	cat build/sub3/list.txt
	echo "Error: Expected only 3 files in list.txt" 1>&2
	exit 1
fi
if ! grep '^[^ ]* [^ ]* [^ ]*\s*$' build2/sub3/list.txt > /dev/null; then
	cat build2/sub3/list.txt
	echo "Error: Expected only 3 files in list.txt" 1>&2
	exit 1
fi

eotup
