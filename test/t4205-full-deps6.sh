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

# Make sure full_deps doesn't think external directories have changed on a
# no-op rebuild.
. ./tup.sh
check_tup_suid

# Re-init in a subdir so we can control the external directory contents.
mkdir external
touch external/foo.txt
mkdir tmp
cd tmp
re_init
set_full_deps

# Run a script twice that does an ls on a directory, so we end up creating the
# entry for 'external' on one invocation and re-using the tent for it on
# another invocation.
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: |> sh run.sh 1 |>
: |> sh run.sh 2 |>
HERE
cat > run.sh << HERE
cat ../external/foo.txt
ls ../external > /dev/null
HERE
tup touch Tupfile foo.c bar.c
update

if [ "$in_windows" = "1" ]; then
	prefix="`echo $PWD | sed 's,/cygdrive/c,c:,'`"
else
	prefix="$PWD"
fi
tup_dep_no_exist $prefix/.. external . 'sh run.sh 1'
tup_dep_no_exist $prefix/.. external . 'sh run.sh 2'

sleep 1
touch ../external/bar.txt
update > .tup/.tupoutput
if ! grep 'No commands to execute' .tup/.tupoutput > /dev/null; then
	cat .tup/.tupoutput
	echo "Error: No files should have been recompiled when nothing was changed." 1>&2
	exit 1
fi

eotup
