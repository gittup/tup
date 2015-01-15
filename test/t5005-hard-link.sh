#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2015  Mike Shal <marfey@gmail.com>
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

echo "[33mSkip t5005 - not needed?[0m"
exit 0

. ./tup.sh
echo 'this is a file' > file1
ln file1 file2
cat > Makefile << HERE
all: new-file1 new-file2
new-%: %
	tup link "cp \$< \$@" -i\$< -o\$@
HERE

tup touch file1 file2 Makefile
update
check_exist new-file1 new-file2

rm new-file1 new-file2

tup touch file1

update
check_exist new-file1 new-file2

eotup
