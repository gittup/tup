#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Make sure we can export environment variables.

export FOO="hey"
. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> sh ok.sh > %o |> out.txt
HERE
cat > ok.sh << HERE
echo "foo is \$FOO"
HERE
update
echo 'foo is ' | diff - out.txt

cat > Tupfile << HERE
export FOO
: |> sh ok.sh > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'foo is hey' | diff - out.txt

export FOO="yo"
update
echo 'foo is yo' | diff - out.txt

cat > Tupfile << HERE
export FOO
: |> ^ run script > %o^ sh ok.sh > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'foo is yo' | diff - out.txt

tup_dep_exist . ok.sh . '^ run script > out.txt^ sh ok.sh > out.txt'
tup_dep_no_exist $ FOO 0 .
tup_dep_exist $ FOO . '^ run script > out.txt^ sh ok.sh > out.txt'

cat > Tupfile << HERE
: |> ^ run script > %o^ sh ok.sh > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'foo is ' | diff - out.txt

# Have to modify the environment variable before tup removes it.
export FOO="latest"
update
tup_object_no_exist $ FOO

eotup
