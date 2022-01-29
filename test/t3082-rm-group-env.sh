#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2022  Mike Shal <marfey@gmail.com>
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

# When we remove an input group, all normal links are removed. However, we
# still need normal links on environment variables so that environment changes
# will trigger the appropriate commands.

. ./tup.sh
export FOO=bar
cat > ok.sh << HERE
cat bar.txt > output.txt && echo \$FOO > env.txt
HERE
cat > Tupfile << HERE
export FOO
: |> cat input.txt > %o |> bar.txt | <group>
: <group> |> sh ok.sh |> output.txt env.txt
HERE
touch input.txt
update

echo bar | diff - env.txt

# Remove the group so all normal links are removed.
cat > ok.sh << HERE
touch output.txt && echo \$FOO > env.txt
HERE
cat > Tupfile << HERE
export FOO
: |> cat input.txt > %o |> bar.txt | <group>
: |> sh ok.sh |> output.txt env.txt
HERE
update

echo bar | diff - env.txt

# Now update the environment variable and make sure we get a new env.txt
export FOO=yo
update

echo yo | diff - env.txt

eotup
