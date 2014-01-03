#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
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

# Try autoupdate with the flag instead of the config option.

. ./tup.sh
check_monitor_supported
monitor --autoupdate
cat > Tupfile << HERE
: foreach *.txt |> (echo '<html>'; cat %f; echo '</html>') > %o |> %B.html
HERE

echo "This is the index" > index.txt
echo "Another page" > page.txt
tup flush

check_exist index.html page.html
cat << HERE | diff index.html -
<html>
This is the index
</html>
HERE

# Change a file and see that it gets updated
echo "Updated index" > index.txt
tup flush
cat << HERE | diff index.html -
<html>
Updated index
</html>
HERE

# Add a new file and see it gets updated
echo "New file" > new.txt
tup flush
cat << HERE | diff new.html -
<html>
New file
</html>
HERE

# Remove a file and see that its dependent file is removed
rm page.txt
tup flush
check_not_exist page.html

eotup
