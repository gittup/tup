#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# At the moment I'm making it a failure to output a file in a different
# directory. Since we can input files from different directories properly,
# which seems to be more useful, it would be weird to also output files in a
# different directory.

. ./tup.sh
tmkdir bar
cat > Tupfile << HERE
: |> echo hey %o |> bar/foo.o
HERE
tup touch Tupfile
update_fail
tup_object_no_exist . "echo hey bar/foo.o"
tup_object_no_exist bar "foo.o"

eotup
