#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# Try to use the .gitignore directive

. ./tup.sh
cat > Tuprules.tup << HERE
.gitignore
HERE

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog.exe
include_rules
HERE
echo 'int main(void) {return 0;}' > foo.c

mkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f |> libsub.a
include_rules
HERE

touch bar.c sub/shazam.c
update

if [ ! -f .gitignore ]; then
	echo "Error: .gitignore file not generated" 1>&2
	exit 1
fi
if [ ! -f sub/.gitignore ]; then
	echo "Error: sub/.gitignore file not generated" 1>&2
	exit 1
fi

gitignore_bad foo.c .gitignore
gitignore_bad bar.c .gitignore
gitignore_bad Tupfile .gitignore
gitignore_good foo.o .gitignore
gitignore_good bar.o .gitignore
gitignore_good .gitignore .gitignore
gitignore_good prog.exe .gitignore
gitignore_bad shazam.c sub/.gitignore
gitignore_bad Tupfile sub/.gitignore
gitignore_good shazam.o sub/.gitignore
gitignore_good libsub.a sub/.gitignore
gitignore_good .gitignore sub/.gitignore

rm -f Tuprules.tup
update
for f in .gitignore sub/.gitignore .gitignore.new sub/.gitignore.new; do
	if [ -f $f ]; then
		echo "Error: $f exists when it shouldn't" 1>&2
		exit 1
	fi
done

eotup
