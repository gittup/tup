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

# Make sure we can't include a Tupfile by using a generated symlink. If we
# could, we'd have to go from the update phase back to the create phase, which
# would be silly. Let's *not* do the time warp again.

. ./tup.sh
check_no_windows symlink

# Make the symlink first, in a separate directory. That way it will exist
# and not be marked delete when we create a new Tupfile in the top-level
mkdir foo
echo 'var = 3' > foo/x86.tup
cat > foo/Tupfile << HERE
: x86.tup |> ln -s %f %o |> arch.tup
HERE
update

cat > Tupfile << HERE
include foo/arch.tup
HERE
update_fail_msg "Unable to read from generated file"

eotup
