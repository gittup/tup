#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Make sure we can remove the tup.config file to clear all the variables.

. ./tup.sh
varsetall FOO=n BAR=n
tup read
tup_object_exist tup.config FOO
tup_object_exist tup.config BAR

vardict_exist FOO
vardict_exist BAR

rm tup.config
tup read

tup_object_no_exist tup.config BAR
tup_object_no_exist tup.config FOO

vardict_no_exist BAR
vardict_no_exist FOO

eotup
