#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# If the monitor sees a directory get removed and turned into a ghost, it will
# have a tup_entry corresponding to a ghost. If an update then removes that
# ghost, the monitor needs to make sure it isn't being kept around.
. ./tup.sh
check_monitor_supported

monitor

mkdir a
mkdir b
mkdir c

echo '#include "foo.h"' > ok.c

touch c/foo.h

cat > Tupfile << HERE
: foreach *.c |> gcc -Ia -Ib -Ic -c %f -o %o |> %B.o
HERE
update

check_exist ok.o

# Now remove everything, so the monitor creates ghosts.
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo "" > ok.c
rm -rf a b c
# Here the monitor has 'a', 'b', and 'c' as ghosts. The following update will
# remove the ghosts, so the monitor should remove those entries.
update

# Re-creating stuff over the (now removed) ghosts should work fine and we
# should be able to update.
mkdir a
mkdir b
mkdir c
touch c/foo.h

update

stop_monitor

eotup
