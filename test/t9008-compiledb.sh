#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2024  Mike Shal <marfey@gmail.com>
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

# Test for 'tup compiledb' to generate compile_commands.json

. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch foo.c bar.c
compiledb 2> .tup/output.txt

if ! grep 'tup warning: No commands exported' .tup/output.txt > /dev/null; then
	echo "Error: Expected warning for no commands." 1>&2
	exit 1
fi

cat > Tupfile << HERE
: foreach *.c |> ^j^ gcc -c %f -o %o |> %B.o
HERE
compiledb

for i in foo bar; do
	if ! grep "gcc -c $i\\.c -o $i\\.o" compile_commands.json > /dev/null; then
		echo "Error: Expected gcc command in compile_commands.json" 1>&2
		exit 1
	fi
done

if [ "$in_windows" = "1" ]; then
	# Make sure backslashes in the directory paths are escaped. (The 8
	# become 4 on the commandline, which become 2 for the grep)
	gitignore_good "directory.*test\\\\\\\\tuptesttmp" compile_commands.json
else
	gitignore_good "directory.*test/tuptesttmp" compile_commands.json
fi

eotup
