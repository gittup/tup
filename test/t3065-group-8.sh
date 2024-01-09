#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# When a command takes over an output from an old command (here, 'sh ok1.sh'
# takes output2.txt from 'sh ok2.sh') we have to make sure we remove the link
# from the output to its old group.
. ./tup.sh

echo 'touch output.txt' > ok1.sh
echo 'touch output2.txt' > ok2.sh
cat > Tupfile << HERE
: |> sh ok1.sh |> output.txt
: |> sh ok2.sh |> output2.txt | <bar>
HERE
update

cat > ok1.sh << HERE
touch output.txt
touch output2.txt
HERE
cat > Tupfile << HERE
: |> sh ok1.sh |> output.txt output2.txt
HERE
update

echo 'touch output.txt' > ok1.sh
echo 'touch output2.txt' > ok2.sh
cat > Tupfile << HERE
: |> sh ok1.sh |> output.txt | <bar>
: |> sh ok2.sh |> output2.txt | <bar>
HERE
update

eotup
