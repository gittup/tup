#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2017  Mike Shal <marfey@gmail.com>
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

# Try to generate a shell script with generated output directories.

. ./tup.sh

tmkdir sub1
cat > sub1/Tupfile << HERE
: |> touch %o |> out/dir/foo.txt
: |> touch %o |> out/bar.txt
: |> touch %o |> out/dir/baz.txt
HERE
tup touch sub1/Tupfile

generate $generate_script_name
./$generate_script_name

# Make sure running it again doesn't fail mkdirs
./$generate_script_name
check_exist sub1/out/dir/foo.txt sub1/out/bar.txt sub1/out/dir/baz.txt

if ! grep 'mkdir' $generate_script_name | wc -l | grep 2 > /dev/null; then
	echo "Error: Generated script should have 2 mkdirs" 1>&2
	exit 1
fi

eotup
