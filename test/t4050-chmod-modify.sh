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

# Make sure that changing attributes counts as being modified.

. ./tup.sh
# The stat.st_ctime field is the file creation time in Windows, so we use
# st_mtime instead. However, st_mtime isn't modified when permissions are
# changed. We should be able to get this information from FILE_BASIC_INFORMATION (
# http://msdn.microsoft.com/en-us/library/windows/hardware/ff545762%28v=vs.85%29.aspx
# ), but I don't know how to actually call a function to retrieve it.
check_no_windows TODO - ctime

cat > ok.sh << HERE
echo foo
HERE
chmod +x ok.sh
cat > Tupfile << HERE
: |> ./ok.sh > %o |> bar
HERE
update

sleep 1

chmod -x ok.sh
update_fail_msg "ok.sh.*Permission denied"

eotup
