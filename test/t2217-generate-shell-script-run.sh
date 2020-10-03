#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020  Michael Kuc <michaelkuc6@gmail.com>
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

# Try to use a 'run' command in a generate script.

. ./tup.sh
check_no_windows run-script

# 'tup generate' with run command requires a tup directory

printf '#! /bin/sh -e\n printf ": |> printf a > output.txt |> output.txt\n"' > script.sh
chmod +x script.sh
cat > Tupfile << HERE
run ./script.sh
HERE
generate $generate_script_name
./$generate_script_name
printf a | diff - output.txt

eotup
