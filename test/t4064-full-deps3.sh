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

# This test makes sure that we update mtimes on unreachable files. Here we create a temporary file
# under a directory in /tmp which we use for an update. Then we will remove the directory containing
# that file. The tmpfile will be removed from the graph, since when the command re-executes it won't
# be able to open the containing directory. However, there is now a dependency from the tup-t4064*
# node to the command, so if we re-create the tmpfile then we still add a new node.
. ./tup.sh
check_no_windows tmp
check_tup_suid

set_full_deps

tmpdir="/tmp/tup-t4064-$$"
cleanup()
{
	cd /tmp
	rm -rf $tmpdir
	cd - > /dev/null
}

trap cleanup EXIT INT TERM
cleanup
mkdir $tmpdir

echo 'hey' > $tmpdir/tmpfile

cat > Tupfile << HERE
: |> ^ Read tmpfile^ if [ -f $tmpdir/tmpfile ]; then cat $tmpdir/tmpfile; else echo nofile; fi > %o |> out.txt
HERE
update
echo 'hey' | diff - out.txt

cleanup
update
echo 'nofile' | diff - out.txt

mkdir $tmpdir
echo 'yo' > $tmpdir/tmpfile
update
echo 'yo' | diff - out.txt

eotup
