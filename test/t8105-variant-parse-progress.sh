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

# Make sure the parser progress bar is correct when removing a variant subdir
# with a ghost in it. The ghost prevents the variant directory from being
# deleted, so we end up with a ghost in the parser graph. The ghost gets
# upgraded to a directory, so we have to make sure things are still being
# counted correctly.

. ./tup.sh
check_no_windows variant

cat .tup/options << HERE
[display]
job_numbers = 1
HERE

mkdir -p sub/a/b sub/c
echo "void bar(void) {}" > sub/a/bar.c
echo "int main(void) {return 0;}" > sub/a/b/foo.c
echo "void zap(void) {}" > sub/c/zap.c
echo "tup.foreach_rule('*.c', 'gcc -c %f -o %o', '%B.o')" > Tupdefault.lua
cat > sub/Tupfile.lua << HERE
tup.rule({}, 'if [ -e a/foo.txt ]; then echo yes; else echo no; fi', {})
tup.rule({'a/bar.o', 'a/b/foo.o', 'c/zap.o'}, 'gcc %f -o %o', 'prog')
HERE

mkdir build-foo
touch build-foo/tup.config
mkdir build-bar
touch build-bar/tup.config
update

rm -rf build-foo/sub/a
tup parse > .tup/.tupoutput

if ! grep '5) ' .tup/.tupoutput > /dev/null; then
	cat .tup/.tupoutput
	echo "Error: Expected 5) in parser output" 1>&2
	exit 1
fi

eotup
