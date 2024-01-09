#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# Have multiple commands share the same exclusion string and output to a group.

. ./tup.sh

cat > Tupfile << HERE
: |> sh run.sh output.txt |> output.txt ^/ignore <group>
: |> sh run.sh output2.txt |> output2.txt ^/ignore <group>
: <group> |> cat output.txt output2.txt |>
HERE
cat > run.sh << HERE
touch \$1
touch ignore1
touch ignore2
mkdir -p ignoredir
touch ignoredir/file.txt
HERE
update

check_exist ignore1
check_exist ignore2
check_exist ignoredir/file.txt

# Make sure a subsequent update doesn't do anything, since all ignored files
# should be in the database.
update_null "All ignored files should be in the database"

# Make sure running with ignored files already in the db still works.
touch run.sh
update

# Now remove the group.
cat > Tupfile << HERE
: |> sh run.sh output.txt |> output.txt ^/ignore
: |> sh run.sh output2.txt |> output2.txt ^/ignore
: output.txt output2.txt |> cat output.txt output2.txt |>
HERE
update

tup_object_exist ^ '/ignore'
tup_object_no_exist . '<group>'

# Now remove one of the rules with the exclusion.
cat > Tupfile << HERE
: |> sh run.sh output.txt |> output.txt ^/ignore
HERE
update

tup_object_exist ^ '/ignore'
tup_dep_exist . 'sh run.sh output.txt' ^ '/ignore'

# When we remove the last rule with an exclusion, the exclusion should be
# removed.
cat > Tupfile << HERE
HERE
update

tup_object_no_exist ^ '/ignore'

eotup
