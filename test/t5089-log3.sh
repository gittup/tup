#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# Test --debug-logging skip files
. ./tup.sh

cat > Tupfile << HERE
: |> ^o^ sh ok.sh |> out.txt same.txt
: foreach *.txt |> cat %f |>
HERE
cat > ok.sh << HERE
echo foo > out.txt
echo bar > same.txt
HERE
update

cat > ok.sh << HERE
echo changed > out.txt
echo bar > same.txt
HERE
update --debug-logging
log_good "Skip file.*: same.txt"
log_good "Skip cmd.*: cat same.txt"

eotup
