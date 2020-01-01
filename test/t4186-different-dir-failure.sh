#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2020  Mike Shal <marfey@gmail.com>
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

# Make sure that if we haven't yet created a generated file in a generated
# directory, that we don't continually reparse the srcid Tupfile.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo/out.txt
HERE
tup parse

tup scan
if ! tup create_flags_exists; then
	echo "Error: Nodes shouldn't have create flags set" 1>&2
	exit 1
fi

eotup
