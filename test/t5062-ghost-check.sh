#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2024  Mike Shal <marfey@gmail.com>
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

# Quick test to make sure that ghost_check won't remove real nodes. Since
# ghost_check is only for debugging, it doesn't get used very often. In theory
# I should put an unused ghost in there to make sure it gets removed, but I
# would need to fiddle with the db directly, or add more debugging
# functionality to tup.

. ./tup.sh
touch foo
tup scan
tup_object_exist . foo

tup ghost_check
tup_object_exist . foo

eotup
