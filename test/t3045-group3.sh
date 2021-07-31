#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# Make sure things work as expected when the output is no longer in the group.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B <foo-autoh>
: foreach *.c | <foo-autoh> |> gcc -c %f -o %o |> %B.o {objs}
HERE
echo '#define FOO 3' > foo.h.in
echo '#define BAR 4' > bar.h.in
cat > foo.c << HERE
#include "foo.h"
#include "bar.h"
HERE
echo '#include "bar.h"' > bar.c
update

tup_dep_exist . 'foo.h' . '<foo-autoh>'
tup_dep_exist . 'bar.h' . '<foo-autoh>'
tup_dep_exist . 'cp foo.h.in foo.h' . '<foo-autoh>'
tup_dep_exist . 'cp bar.h.in bar.h' . '<foo-autoh>'

# If we remove our output group, then the compilation commands should
# re-execute and fail because they don't include the generated headers as
# dependencies.
cat > Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B
: foreach *.c | <foo-autoh> |> gcc -c %f -o %o |> %B.o {objs}
HERE
update_fail_msg "Missing input dependency"

tup_dep_no_exist . 'foo.h' . '<foo-autoh>'
tup_dep_no_exist . 'bar.h' . '<foo-autoh>'
tup_dep_no_exist . 'cp foo.h.in foo.h' . '<foo-autoh>'
tup_dep_no_exist . 'cp bar.h.in bar.h' . '<foo-autoh>'

# Adding the outputs back in the group should work.
cat > Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B <foo-autoh>
: foreach *.c | <foo-autoh> |> gcc -c %f -o %o |> %B.o {objs}
HERE
update

tup_dep_exist . 'foo.h' . '<foo-autoh>'
tup_dep_exist . 'bar.h' . '<foo-autoh>'
tup_dep_exist . 'cp foo.h.in foo.h' . '<foo-autoh>'
tup_dep_exist . 'cp bar.h.in bar.h' . '<foo-autoh>'

eotup
