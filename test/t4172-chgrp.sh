#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015  Mike Shal <marfey@gmail.com>
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

# Same as t4031, but with chgrp.

. ./tup.sh
check_no_windows shell
if ! whoami | grep marf > /dev/null; then
	echo "[33mSkip t4172 - you're not marf.[0m"
	eotup
fi

cat > Tupfile << HERE
ifeq (@(TUP_PLATFORM),macosx)
group = staff
else
group = marf
endif
: |> touch %o; chgrp \$(group) %o |> test1
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
ifeq (@(TUP_PLATFORM),macosx)
group = staff
else
group = marf
endif
: |> chgrp \$(group) test2 |>
HERE
tup touch Tupfile test2
update_fail_msg "tup error.*chown"

eotup
