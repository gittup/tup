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

# While trying to generate some of the files in gcc, I ended up with a weird
# case where I generate the same file twice (here, errors.o), and in one case
# use that to create an exe that ends up creating a generated header. That
# generated header is then used as an input in the second exe. This
# demonstrates two issues:
#
# 1) Two different rules shouldn't create the same command
# 2) Circular dependencies aren't getting detected.
#
# The result instead was a fatal tup error since the graph isn't empty.

. ./tup.sh
cat > Tupfile << HERE
!cc = | \$(generated_headers) |> gcc -c %f -o %o |>
: foreach errors.c |> !cc |> errors.o
: errors.o |> gcc %f -o %o |> errors1
generated_headers = foo.h
: errors1 |> cat errors1 > /dev/null && echo '#define FOO 3' > %o |> foo.h
: foreach errors.c |> !cc |> errors.o
: errors.o |> gcc %f -o %o |> errors2
HERE
echo 'int main(void) {return 0;}' > errors.c
update_fail_msg "Unable to create output file 'errors.o'"

eotup
