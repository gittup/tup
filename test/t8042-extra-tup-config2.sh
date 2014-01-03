#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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

# Try to create an extra tup.config in the root directory while removing a variant.
. ./tup.sh
check_no_windows variant

tmkdir build-debug
tmkdir build-default
tmkdir sub

cat > Tupfile << HERE
.gitignore
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o sub/*.o |> gcc %f -o %o |> prog
HERE
cat > sub/Tupfile << HERE
.gitignore
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
tup touch Tupfile foo.c build-default/tup.config build-debug/tup.config sub/bar.c
update

for i in foo.o sub/bar.o prog sub/.gitignore; do
	check_exist build-debug/$i
	check_exist build-default/$i
	check_not_exist $i
done
check_exist build-debug/.gitignore
check_exist build-default/.gitignore
check_exist .gitignore

tup touch tup.config
rm build-debug/tup.config
tup parse > .output.txt

for i in foo.o sub/bar.o prog sub/.gitignore; do
	check_not_exist build-debug/$i
	check_exist build-default/$i
	check_not_exist $i
done
check_not_exist build-debug/.gitignore
check_exist build-default/.gitignore
check_exist .gitignore

if grep "build-default/sub" .output.txt > /dev/null; then
	cat .output.txt
	echo "Error: Shouldn't be re-parsing Tupfiles" 1>&2
	exit 1
fi

eotup
