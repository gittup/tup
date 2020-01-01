#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2020  Mike Shal <marfey@gmail.com>
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

# I think this is an issue: If I make a new Tupfile dependent on another
# Tupfile, which is depdenent on a third Tupfile (so A -> B -> C, and C is the
# new one...meaning to parse C I have to parse B first, etc), and I have
# created C and added a file to A at the same time, then C may be parsed first
# and not try to parse B because it is already parsed, and I'm just checking
# create flags at the moment, instead of its presence in the DAG.
. ./tup.sh

# Need to make sure C runs first when it comes up (seems to go in order of ID)
tmkdir C
cat > C/Tupfile << HERE
HERE
tup touch C/Tupfile

tmkdir A
cat > A/Tupfile << HERE
: |> echo hey > %o |> foo.txt
HERE

tmkdir B
cat > B/Tupfile << HERE
: foreach ../A/*.txt |> cp %f %o |> %b
HERE

tup touch A/Tupfile B/Tupfile
update
check_exist A/foo.txt B/foo.txt

cat > C/Tupfile << HERE
: foreach ../B/*.txt |> cp %f %o |> %b
HERE
cat > A/Tupfile << HERE
: |> echo hey > %o |> foo.txt
: |> echo yo > %o |> bar.txt
HERE
tup touch C/Tupfile
tup touch A/Tupfile
update
check_exist A/foo.txt A/bar.txt B/foo.txt B/bar.txt C/foo.txt C/bar.txt

eotup
