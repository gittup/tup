#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2015  Mike Shal <marfey@gmail.com>
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

# Make sure stderr from a run-script is displayed under the correct banner
. ./tup.sh
check_no_windows run-script

cat > run.sh << HERE
echo "Run script \$1" 1>&2
HERE
chmod +x run.sh

tmkdir sub1
tmkdir sub2
cat > sub1/Tupfile << HERE
run ../run.sh part1A
: ../sub2/foo.txt |> echo %f |>
run ../run.sh part1B
HERE

cat > sub2/Tupfile << HERE
run ../run.sh part2
HERE

tup touch sub1/Tupfile sub2/Tupfile sub2/foo.txt
update > .output.txt 2>&1

if ! cat .output.txt | tr '\n' ' ' | grep '2).*sub2.*part2.*3).*sub1.*part1A.*part1B' > /dev/null; then
	cat .output.txt
	echo "Error: Expected 'part2' under sub2, and 'part1[AB]' under sub1"
	exit 1
fi

eotup
