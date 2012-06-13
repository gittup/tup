#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

# Make sure groups actually enforce ordering.

. ./tup.sh
single_threaded
cat > Tupfile << HERE
: foreach *.c | <foo-autoh> |> gcc -c %f -o %o |> %B.o {objs}
: foreach *.h.in |> cp %f %o |> %B <foo-autoh>
HERE
echo '#define FOO 3' > foo.h.in
cat > foo.c << HERE
#include "foo.h"
HERE
update

eotup
