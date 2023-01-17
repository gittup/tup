#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2023  Mike Shal <marfey@gmail.com>
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

# Check to make sure we get the warnings from hidden files.
# (This behavior of tup may be stupid).

. ./tup.sh

# In case the user disabled this, we make sure warnings are on here.
cat >> .tup/options << HERE
[updater]
warnings = 1
HERE
cat > Tupfile << HERE
: |> touch .hg |>
: |> touch .git |>
HERE
if tup upd 2>&1 | grep "Update resulted in 2 warnings" > /dev/null; then
	:
else
	echo "Error: Expected 2 warnings." 1>&2
	exit 1
fi

eotup
