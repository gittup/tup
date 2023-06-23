#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2023  Mike Shal <marfey@gmail.com>
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

# Test for ignoring files that are outside of the tup hierarchy with full_deps
# enabled.

. ./tup.sh
check_tup_suid

origdir=$PWD
mkdir external
mkdir tmp
cd tmp
re_init
set_full_deps

cat > Tupfile << HERE
: |> sh ok.sh |> out.txt | ^/tuptestlog.txt
HERE
cat > ok.sh << HERE
echo foo > $origdir/external/tuptestlog.txt
echo foo > $origdir/external/tupextrafile.txt
echo bar > out.txt
HERE
update_fail_msg "File.*tupextrafile.txt.*was written to"

cat > Tupfile << HERE
: |> sh ok.sh |> out.txt | ^/tuptestlog.txt ^/tupextrafile
HERE
update

eotup
