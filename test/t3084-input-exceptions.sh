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

# Special input syntax to exclude some specific nodes from, for example,
# foreach scope

. ./tup.sh

echo ': *.file ^c.file |> cat %f > %o |> lines.txt' > Tupfile

for i in a b c d; do
	echo $i > $i.file
done
update

gitignore_good a lines.txt
gitignore_good b lines.txt
gitignore_bad c lines.txt
gitignore_good d lines.txt

echo ': *.file ^e.file |> cat %f > %o |> lines.txt' > Tupfile

tup touch Tupfile
update

gitignore_good a lines.txt
gitignore_good b lines.txt
gitignore_good c lines.txt
gitignore_good d lines.txt

echo ': *.file ^[bc].file |> cat %f > %o |> lines.txt' > Tupfile

tup touch Tupfile
update

gitignore_good a lines.txt
gitignore_bad b lines.txt
gitignore_bad c lines.txt
gitignore_good d lines.txt

echo ': *.file ^c.file ^[bc].file |> cat %f > %o |> lines.txt' > Tupfile

tup touch Tupfile
update

gitignore_good a lines.txt
gitignore_bad b lines.txt
gitignore_bad c lines.txt
gitignore_good d lines.txt

echo ': sub/*.file ^sub/c.file |> cat %f > %o |> lines.txt' > Tupfile

mkdir sub
mv *.file sub/

tup touch Tupfile
update

gitignore_good a lines.txt
gitignore_good b lines.txt
gitignore_bad c lines.txt
gitignore_good d lines.txt

eotup
