#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
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

# See if we can override a bang macro using an alias

. ./tup.sh
cat > Tupfile << HERE
!touch = |> cat %f && touch %o |>
!cc = |> gcc -c %f -o %o |>

!mplayer.c = !touch
!mplayer.asm = !touch

# Now override
!mplayer.c = !cc

files += foo.c
files += bar.asm
: foreach \$(files) |> !mplayer |> %B.o
HERE
tup touch foo.c bar.asm Tupfile
update

check_exist foo.o bar.o
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.asm . 'cat bar.asm && touch bar.o'

eotup
