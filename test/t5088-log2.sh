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

# Test --debug-logging graphs
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -Ia -Ib -c %f -o %o |> %B.o
HERE
echo '#include "foo.h"' > bar.c
echo '#include "foo.h"' > foo.c
mkdir a
mkdir b
touch b/foo.h
update --debug-logging

log_graph_good create "label=\"\."
log_graph_good update "gcc -Ia -Ib -c bar.c -o bar.o"
log_graph_good update "gcc -Ia -Ib -c foo.c -o foo.o"

tup touch bar.c
update --debug-logging

log_graph_bad create "label=\"\."
log_graph_good update "gcc -Ia -Ib -c bar.c -o bar.o"
log_graph_bad update "gcc -Ia -Ib -c foo.c -o foo.o"

touch a/foo.h
update --debug-logging
log_good "Create(overwrite ghost).*a[/\]foo.h"

mkdir overfile
update
rmdir overfile
touch overfile
update --debug-logging
log_good "Create(overwrite).*overfile.*oldtype=2"

PATH=$PATH:$PWD update --debug-logging
log_good "Env update.*PATH"
check_exist .tup/log/update.dot.0
update

update --debug-logging
if ! wc -l .tup/log/debug.log.0  | grep '1 \.tup/log/debug.log.0' > /dev/null; then
	echo "Error: Expected only one line in debug.log.0" 1>&2
	exit 1
fi

eotup
