#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# Tup has a limit of 8 nested if statements before borking.

. ./tup.sh

cat > Tupfile << HERE
ifeq (a,1)
ifeq (a,2)
ifeq (a,3)
ifeq (a,4)
ifeq (a,5)
ifeq (a,6)
ifeq (a,7)
ifeq (a,8)
endif
endif
endif
endif
endif
endif
endif
endif
HERE
tup touch Tupfile
parse

cat > Tupfile << HERE
ifeq (a,1)
ifeq (a,2)
ifeq (a,3)
ifeq (a,4)
ifeq (a,5)
ifeq (a,6)
ifeq (a,7)
ifeq (a,8)
ifeq (a,9)
endif
endif
endif
endif
endif
endif
endif
endif
endif
HERE
tup touch Tupfile
parse_fail_msg "too many nested if statements"

eotup
