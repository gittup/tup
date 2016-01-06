#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2016  Mike Shal <marfey@gmail.com>
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

# Force tup to die in order to test unmounting the file-system before
# mounting a new one.
. ./tup.sh

check_no_windows shell

# Ignore errors, since the killed tup makes for a bad-looking error message.
# We check the return value of tup below.
exec 2>/dev/null

cat > ok.sh << HERE
while [ ! -f pid.txt ]; do true; done
kill -9 \`cat pid.txt\`
HERE
cat > Tupfile << HERE
: |> ./ok.sh %o |> output.txt
HERE

chmod +x ok.sh
tup touch ok.sh Tupfile
tup upd &
pid=$!
echo $pid > pid.txt

if wait $pid; then
	echo "Error: Expected the spawned tup process to fail." 1>&2
	exit 1
fi

# Now the file-system should still be mounted, so try to run tup again
# (now with a good script) and see that we can update successfully.
cat > ok.sh << HERE
echo "All good." > \$1
HERE
tup touch ok.sh
update

eotup
