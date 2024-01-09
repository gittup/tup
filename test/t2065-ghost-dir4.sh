#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Like t2040, but now with a longer chain of ghosts.

. ./tup.sh
cat > ok.sh << HERE
cat super/secret/ghost 2>/dev/null || echo nofile
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
update
tup_object_exist . super

rm -f Tupfile
update

tup_object_no_exist . super
tup_object_no_exist super secret
tup_object_no_exist super/secret ghost

eotup
