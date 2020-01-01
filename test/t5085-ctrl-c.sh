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

# Make sure if we ctrl-C tup, that all subprocesses are dead and we still make
# progress on the completed jobs.
. ./tup.sh
check_no_windows process

mypid=$$
cat > Tupfile << HERE
: |> sh ctrlctup-$mypid.sh 0.1 |>
: |> sh ctrlctup-$mypid.sh 0.2 |>
: |> sh ctrlctup-$mypid.sh 0.3 |>
: |> sh ctrlctup-$mypid.sh 2.0 && touch %o |> outfile
HERE
cat > ctrlctup-$mypid.sh << HERE
sleep \$1
HERE
set +e
tup upd > /tmp/tup-out1-$$.txt 2>&1 &
pid=$!
sleep 1
kill $pid
if wait $pid; then
	echo "Error: waiting on tup process should have failed." 1>&2
	exit 1
fi
if pgrep -f ctrlctup-$mypid.sh; then
	ps -Af | grep ctrlctup-$mypid.sh
	echo "Error: Subprocess is still running." 1>&2
	exit 1
fi
if grep 'Expected to write to file.*outfile' /tmp/tup-out1-$$.txt > /dev/null; then
	cat /tmp/tup-out1-$$.txt
	echo "Error: Expected not to receive error message for missing outfile" 1>&2
	exit 1
fi
tup todo > /tmp/tup-out2-$$.txt
if ! grep 'The following 1 command' /tmp/tup-out2-$$.txt > /dev/null; then
	cat /tmp/tup-out2-$$.txt
	echo "Error: Expecting 1 command to update" 1>&2
	exit 1
fi
rm -f /tmp/tup-out1-$$.txt /tmp/tup-out2-$$.txt

eotup
