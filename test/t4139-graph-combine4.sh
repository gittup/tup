#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Try to combine a chain of commands
. ./tup.sh

cat > ok.sh << HERE
cat \$1; touch \$2
HERE
cat > Tupfile << HERE
: |> sh ok.sh foo.idl %o |> foo.h
: |> sh ok.sh bar.idl %o |> bar.h
: foo.h |> cp %f %o |> outdir/%b | <generated-headers>
: bar.h |> cp %f %o |> outdir/%b | <generated-headers>
HERE
touch foo.idl bar.idl
update

tup graph . --combine > ok.dot
gitignore_good 'node.*\(\.idl\|ok.sh\).*3 files' ok.dot
gitignore_good 'node.*sh ok.sh.*idl.*2 commands' ok.dot
gitignore_good 'node.*\.h.*2 files' ok.dot
gitignore_good 'node.*cp.*\.h.*2 commands' ok.dot

eotup
