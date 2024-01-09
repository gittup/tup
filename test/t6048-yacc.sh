#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2024  Mike Shal <marfey@gmail.com>
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

# Mimic an old-school tool like yacc which uses the same temp file for every
# invocation.

. ./tup.sh
check_no_windows shell
check_no_ldpreload tmpfile

cat > ok.sh << HERE
echo "\$1" > tmp.txt
mv tmp.txt \$2
HERE
chmod +x ok.sh
cat > Tupfile << HERE
: foreach *.in |> ./ok.sh %f %o |> %B.out
HERE
touch 0.in 1.in 2.in 3.in 4.in 5.in 6.in 7.in 8.in 9.in
update -j10

for i in `seq 0 9`; do
	if ! grep $i.in $i.out > /dev/null; then
		echo "Error: $i.out should contain \"$i.in\"" 2>&1
		exit 1
	fi
done

eotup
