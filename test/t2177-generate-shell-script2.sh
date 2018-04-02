#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2018  Mike Shal <marfey@gmail.com>
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

# Same as t2175, but make sub1 the Tupfile that links the library. This fiddles
# around with the parser order to make sure we don't get spurious circular
# dependency errors.

. ./tup.sh

tmkdir sub1
tmkdir sub2
cat > Tuprules.tup << HERE
: foreach *.c |> ^ CC %f^ gcc -c %f -o %o |> %B.o
HERE
echo 'int foo;' > foo.c
echo 'int bar;' > sub1/bar.c
echo 'int bar2;' > sub1/bar2.c
echo 'int baz;' > sub2/baz.c
echo 'int baz2;' > sub2/baz2.c
echo 'include_rules' > Tupfile
cp Tupfile sub1
cp Tupfile sub2
echo ': *.o ../*.o ../sub2/*.o |> ar cr %o %f |> libfoo.a' >> sub1/Tupfile

case $tupos in
CYGWIN*)
	expected="@echo ON"
	;;
*)
	expected="^#! /bin/sh -ex$"
	;;
esac

generate --verbose $generate_script_name
./$generate_script_name
sym_check sub1/libfoo.a foo bar bar2 baz baz2

if ! grep "$expected" $generate_script_name > /dev/null; then
	echo "Error: Expected $expected in generated script" 1>&2
	exit 1
fi

eotup
