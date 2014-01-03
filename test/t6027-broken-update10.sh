#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# Apparently there's a parser bug! That's what I get for not using a real
# parser generator. In the second line of the Tupfile, the {objs} slurps too
# many characters, so it will look for the file "ib" instead of "lib".
#
# I don't care about the lib, so I just use ls instead of a linking command.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} lib |> ls %f |>
HERE
tup touch Tupfile foo.c bar.c lib
update

eotup
