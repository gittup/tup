#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

# Try to rm -rf multiple variants.
. ./tup.sh
check_no_windows variant

tmkdir sub
tmkdir configs
tmkdir build-default
tmkdir build-debug

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch build-default//tup.config build-debug/tup.config sub/Tupfile sub/foo.c sub/bar.c

update

touch configs/foo.config
touch configs/bar.config
touch configs/baz.config
touch configs/zap.config
touch configs/zng.config
tup variant configs/*.config
update

rm -rf build-foo build-bar build-baz build-zap build-zng
update

for i in foo bar baz zap zng; do
	tup_object_no_exist . build-$i
done

eotup
