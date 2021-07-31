#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2021  Mike Shal <marfey@gmail.com>
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

# Like t2188, but in a variant

. ./tup.sh

cat > Tupfile << HERE
: foreach *.txt |> !tup_ln |> %B.lnk
HERE
mkdir build
echo foo > foo.txt
echo bar > bar.txt
touch foo.txt bar.txt build/tup.config

mkdir sub
cat > sub/Tupfile << HERE
: foreach *.txt |> !tup_ln |> %B.lnk
HERE
echo baz > sub/baz.txt
echo blah > sub/blah.txt
touch sub/baz.txt sub/blah.txt
update

tup_dep_exist build "$(tup_ln_cmd foo.txt build/foo.lnk)" build foo.lnk
tup_dep_exist build "$(tup_ln_cmd bar.txt build/bar.lnk)" build bar.lnk

tup_dep_exist build/sub "$(tup_ln_cmd baz.txt ../build/sub/baz.lnk)" build/sub baz.lnk
tup_dep_exist build/sub "$(tup_ln_cmd blah.txt ../build/sub/blah.lnk)" build/sub blah.lnk

case $tupos in
	CYGWIN*)
		# Windows does a copy, so it should have input dependencies
		tup_dep_exist . foo.txt build "$(tup_ln_cmd foo.txt build/foo.lnk)"
		tup_dep_exist . bar.txt build "$(tup_ln_cmd bar.txt build/bar.lnk)"

		tup_dep_exist sub baz.txt build/sub "$(tup_ln_cmd baz.txt ../build/sub/baz.lnk)"
		tup_dep_exist sub blah.txt build/sub "$(tup_ln_cmd blah.txt ../build/sub/blah.lnk)"
		;;
	*)
		# Other platforms use symlink, so no input dependencies
		tup_dep_no_exist . foo.txt build "$(tup_ln_cmd foo.txt build/foo.lnk)"
		tup_dep_no_exist . bar.txt build "$(tup_ln_cmd bar.txt build/bar.lnk)"

		tup_dep_no_exist sub baz.txt build/sub "$(tup_ln_cmd baz.txt ../build/sub/baz.lnk)"
		tup_dep_no_exist sub blah.txt build/sub "$(tup_ln_cmd blah.txt ../build/sub/blah.lnk)"
		;;
esac

echo foo | diff - build/foo.lnk
echo bar | diff - build/bar.lnk
echo baz | diff - build/sub/baz.lnk
echo blah | diff - build/sub/blah.lnk

eotup
