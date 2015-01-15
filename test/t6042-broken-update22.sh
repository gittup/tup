#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2015  Mike Shal <marfey@gmail.com>
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

# In between updates, we should be able to have a node go from a directory to
# a file and back again.

. ./tup.sh

tmkdir atmp
update

rmdir atmp
touch atmp
update

rm atmp
mkdir atmp
update

eotup
