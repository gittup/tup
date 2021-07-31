#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Make sure a dependent Tupfile that fails still gets re-parsed.

. ./tup.sh
mkdir foo
mkdir bar
cat > foo/Tupfile << HERE
: |> echo yo > %o |> yo.h
HERE
cat > bar/Tupfile << HERE
: ../foo/*.txt |> cp %f %o |> output
: ../foo/*.c |> cp %f %o |> output
HERE
touch foo/ok.txt
update

# Change yo.h to yo.c so the second rule in bar/Tupfile is triggered to also
# hit the same output file (thus causing an error).
cat > foo/Tupfile << HERE
: |> echo yo > %o |> yo.c
HERE
update_fail

# An update should again try to parse bar and fail again.
update_fail

eotup
