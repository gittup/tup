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

# Use %1f, etc with variable inputs

. ./tup.sh
cat > Tupfile << HERE
files += f1.e
files += f2.e
files += f3.e
files += f/f4.e
libs += l1
libs += l2
libs += l3
libs += l4
: test1 \$(files) \$(libs) |> cmd T %1f F %2f b %2b B %2B L %3f O %1o 2 %2o |> \$(empty) out2
HERE
tmkdir f
tup touch test1 f1.e f2.e f3.e f/f4.e l1 l2 l3 l4
parse

tup_object_exist . 'cmd T test1 F f1.e f2.e f3.e f/f4.e b f1.e f2.e f3.e f4.e B f1 f2 f3 f4 L l1 l2 l3 l4 O  2 out2'

eotup
