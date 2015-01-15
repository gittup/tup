#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Include multiple subdirectories of Tupfiles

. ./tup.sh
tmkdir sub1
tmkdir sub1/sub2
tmkdir sub1/sub2/sub3

echo 'include sub1/1.tup' > Tupfile
echo 'include sub2/2.tup' > sub1/1.tup
echo 'include sub3/3.tup' > sub1/sub2/2.tup
echo 'cflags += -DFOO' > sub1/sub2/sub3/3.tup
tup touch Tupfile sub1/1.tup sub1/sub2/2.tup sub1/sub2/sub3/3.tup
update

eotup
