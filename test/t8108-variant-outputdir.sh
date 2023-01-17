#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2023  Mike Shal <marfey@gmail.com>
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

# Test for TUP_VARIANT_OUTPUTDIR
. ./tup.sh

mkdir build

cat > Tuprules.tup << HERE
include rules/stuff.tup
HERE

mkdir rules
cat > rules/stuff.tup << HERE
CFLAGS += CWD=\$(TUP_CWD)
CFLAGS += VARIANTDIR=\$(TUP_VARIANTDIR)
CFLAGS += VARIANT_OUTPUTDIR=\$(TUP_VARIANT_OUTPUTDIR)
HERE

mkdir sub
mkdir sub/dir
cat > sub/dir/Tupfile << HERE
include_rules
: |> echo \$(CFLAGS) > %o |> out.txt
HERE
update

gitignore_good 'CWD=../../rules' sub/dir/out.txt
gitignore_good 'VARIANTDIR=../../rules' sub/dir/out.txt
gitignore_good 'VARIANT_OUTPUTDIR=\.' sub/dir/out.txt

touch build/tup.config
update

gitignore_good 'CWD=../../rules' build/sub/dir/out.txt
gitignore_good 'VARIANTDIR=../../build/rules' build/sub/dir/out.txt
gitignore_good 'VARIANT_OUTPUTDIR=../../build/sub/dir' build/sub/dir/out.txt

eotup
