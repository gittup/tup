#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Similar to t5027, only this time we write a symlink in the wrong spot. This
# is necessary in addition to t5027 because symlinks are handled differently
# than normal output files.
. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
HERE
touch Tupfile
update
echo 'foo' | diff - file1

# Oops - accidentally overwrite file1 with a symlink
cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
: |> touch file2; ln -sf file2 file1 |> file2
HERE
touch Tupfile
update_fail

# The echo 'foo' > file1 command should run again. Note that file1 was a
# symlink to file2, but tup rm file1 so the command should succeed again.
cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
: file1 |> ln -s file1 %o |> file2
HERE
touch Tupfile
update
echo 'foo' | diff - file1

eotup
