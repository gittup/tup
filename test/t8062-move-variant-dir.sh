#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2024  Mike Shal <marfey@gmail.com>
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

# Move a variant directory to the src tree.
. ./tup.sh
check_no_windows tup.config not a symlink

mkdir sub
mkdir configs

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch sub/foo.c sub/bar.c
echo "CONFIG_FOO=y" > configs/foo.config
tup variant configs/*.config
update

tup_object_exist build-foo/tup.config FOO

mv build-foo sub
update_fail_msg "tup error: Please clean out the variant directory of extra files"

# Now remove the extra files from the moved variant so it can be rebuilt.
rm -rf sub/build-foo/configs
rm -rf sub/build-foo/sub
update

# Now we get files built in the new variant.
check_exist sub/build-foo/sub/foo.o sub/build-foo/sub/bar.o

# The new tup.config is actually an invalid symlink since it was created with
# 'tup variant', so we can't read any config variables from it.
tup_object_no_exist sub/build-foo/tup.config FOO

eotup
