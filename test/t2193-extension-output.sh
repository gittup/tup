#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2017  Mike Shal <marfey@gmail.com>
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

# Test the %e extension variable in an output, which is only valid in a foreach
# command.

. ./tup.sh
cat > Tupfile << HERE
EXT_txt = out
EXT_src = obj
: foreach *.src *.txt |> cp %f %o |> %B.\$(EXT_%e)
HERE
tup touch foo.txt bar.txt baz.src
update
tup_object_exist . foo.out
tup_object_exist . bar.out
tup_object_exist . baz.obj

eotup
