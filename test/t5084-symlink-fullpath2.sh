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

# Similar to t5083, but try to trigger the logic in gcc that uses shorter paths
# when canonicalizing them. The interaction here is weird - we have to have a
# header that defines itself as a system header with a GCC pragma, and that
# header needs to include another header. This ends up tricking gcc into
# thinking that a whole directory is a system directory, which ends up using
# the maybe_shorter_path() logic in gcc/libcpp/files.c. This may canonicalize
# paths with realpath() instead of trying to open files directly, so symlinks
# may be read without actually accessing the symlink file via readlink() or
# open().
. ./tup.sh
check_no_windows symlink
check_tup_suid
set_full_deps

cwd=$PWD
cat > foo.c << HERE
#include "longer.h"
HERE
mkdir include
cat > include/Tupfile << HERE
: $cwd/foo.h |> !tup_ln |> longer.h ../<headers>
: $cwd/bar.h |> !tup_ln |> longer2.h ../<headers>
HERE
mcmd="gcc -c foo.c -o foo.o -I$PWD/system -I$PWD/include"
cat > Tupfile << HERE
: | <headers> |> $mcmd |> foo.o
HERE
mkdir system
cat > system/longer.h << HERE
#pragma GCC system_header
#include_next <longer.h>
HERE
cat > foo.h << HERE
#include "longer2.h"
HERE
touch bar.h
update

tup_dep_exist include longer.h . "$mcmd"
tup_dep_exist include longer2.h . "$mcmd"
tup_dep_exist system longer.h . "$mcmd"
tup_dep_exist . foo.h . "$mcmd"
tup_dep_exist . bar.h . "$mcmd"

eotup
