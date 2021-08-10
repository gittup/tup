#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Make sure we use a consistent PATH between a monitor running with autoparse and
# an outside 'tup upd'.

. ./tup.sh
check_monitor_supported

PATH=$PWD/a:$PATH monitor --autoparse
export PATH=$PWD/b:$PATH

mkdir a
mkdir b

cat > a/run.sh << HERE
echo arun >> .tup/.run.out
HERE

cat > b/run.sh << HERE
echo 'brun' >> .tup/.run.out
HERE
chmod +x a/run.sh b/run.sh

cat > Tupfile << HERE
: |> run.sh > %o |> output
HERE
tup

# By now we've autoparsed with the monitor environ, and then run tup which
# builds with the new environ. If we touch the Tupfile to trigger an
# autoparse, we shouldn't get the new environ. Running 'tup' again should just
# quit without doing anything, which we check by looking at the combined output
# of the runs.

touch Tupfile
tup

if [ "$(grep -c arun .tup/.run.out)" != 0 ]; then
    echo "Error: Expected a/run.sh to never run." 1>&2
    cat .tup/.run.out 1>&2
    exit 1
fi

if [ "$(grep -c brun .tup/.run.out)" != 1 ]; then
    echo "Error: Expected b/run.sh to run once." 1>&2
    cat .tup/.run.out 1>&2
    exit 1
fi

eotup
