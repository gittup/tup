#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Run an external script that creates bins, and make sure those bin names
# don't get clobbered by later commands.
. ./tup.sh
check_no_windows run-script

cat > gen.sh << HERE
#!/bin/sh
echo ': |> !touch |> a.txt {touched}'
echo ': foreach a.txt |> !cp |> {copies}'
HERE
chmod +x gen.sh

cat > Tupfile << HERE
!touch = |> touch %o |>
!cp = |> cp %f %o |> %f.copy
run ./gen.sh
: |> !touch |> c.cc c.h {cc}
: foreach *.cc | {cc} |> !cp |> {copies}
: foreach {copies} |> !cp |> {copies2}
HERE
update

check_exist a.txt.copy.copy

eotup
