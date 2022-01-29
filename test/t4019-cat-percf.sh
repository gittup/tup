#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# I used to do a weird thing where I'd append characters after a %f before the
# next space after each filename. So here, "cat %f;" would become "cat foo.txt;
# bar.txt;". I think this was so I could do "ld %F.o" when I didn't have the
# object files in the DAG (ie: this was a long time ago). I don't think it's
# necessary anymore and is confusing.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: foo.txt bar.txt |> cat %f; echo yay |>
HERE
echo "foo" > foo.txt
echo "bar" > bar.txt
update

eotup
