#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2023  Mike Shal <marfey@gmail.com>
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

# Make sure a chain works from an included file

. ./tup.sh
cat > whee.lua << HERE
function build_objs(t, lib)
	for k, v in pairs(t) do
		cc_rule(v)
	end
	return tup.rule(t, 'ld -r %f -o %o', lib)
end

function cc_rule(obj)
	basename = string.match(obj, '(.*)%..*')
	expanded_objs = _G[basename .. '_y']
	if expanded_objs then
		return build_objs(expanded_objs, obj)
	end
	return tup.rule(basename .. '.c', 'gcc -c %f -o %o', '%B.o')
end
HERE
cat > Tupfile.lua << HERE
tup.include('whee.lua')

obj_y += 'foo.o'
obj_y += 'bar.o'
bar_y += 'bar1.o'
bar_y += 'bar2.o'

build_objs(obj_y, 'built-in.o')
HERE
echo 'int main(void) {return 0;}' > foo.c
echo 'void bar1(void) {}' > bar1.c
echo 'void bar2(void) {}' > bar2.c
update

tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar1.c . 'gcc -c bar1.c -o bar1.o'
tup_dep_exist . bar2.c . 'gcc -c bar2.c -o bar2.o'
tup_dep_exist . bar1.o . 'ld -r bar1.o bar2.o -o bar.o'
tup_dep_exist . bar2.o . 'ld -r bar1.o bar2.o -o bar.o'
tup_dep_exist . foo.o . 'ld -r foo.o bar.o -o built-in.o'
tup_dep_exist . bar.o . 'ld -r foo.o bar.o -o built-in.o'

eotup
