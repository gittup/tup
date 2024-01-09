#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022-2024  Mike Shal <marfey@gmail.com>
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

# Test the 's' flag for streaming output.

. ./tup.sh

cat > ok1.sh << HERE
echo "first output"
touch \$1
HERE
cat > ok2.sh << HERE
echo "second output"
HERE

cat > Tupfile << HERE
: |> ^s^ sh ok1.sh %o |> output
: output |> ^s^ sh ok2.sh |>
HERE
update > .tup/output

if ! cat .tup/output | awk '/first output/{x=1} /sh ok1.sh output/{if(x == 1) {x=2}} /second output/{if(x == 2) {x=3}} /sh ok2.sh/{if(x == 3) {x=4}} END{if(x != 4) {print "Expected to match lines in order."; exit 1}}'; then
	echo "Output: "
	cat .tup/output
	exit 1
fi

eotup
