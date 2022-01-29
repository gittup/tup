#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2022  Mike Shal <marfey@gmail.com>
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

# Try the !tup_ln macro

. ./tup.sh

cat > Tupfile << HERE
: foreach *.txt |> !tup_ln |> %B.lnk
HERE
touch foo.txt bar.txt

mkdir sub
cat > sub/Tupfile << HERE
: foreach *.txt |> !tup_ln |> %B.lnk
HERE
touch sub/baz.txt sub/blah.txt
update

tup_dep_exist . "$(tup_ln_cmd foo.txt foo.lnk)" . foo.lnk
tup_dep_exist . "$(tup_ln_cmd bar.txt bar.lnk)" . bar.lnk

tup_dep_exist sub "$(tup_ln_cmd baz.txt baz.lnk)" sub baz.lnk
tup_dep_exist sub "$(tup_ln_cmd blah.txt blah.lnk)" sub blah.lnk

case $tupos in
	CYGWIN*)
		# Windows does a copy, so it should have input dependencies
		tup_dep_exist . foo.txt . "$(tup_ln_cmd foo.txt foo.lnk)"
		tup_dep_exist . bar.txt . "$(tup_ln_cmd bar.txt bar.lnk)"

		tup_dep_exist sub baz.txt sub "$(tup_ln_cmd baz.txt baz.lnk)"
		tup_dep_exist sub blah.txt sub "$(tup_ln_cmd blah.txt blah.lnk)"
		;;
	*)
		# Other platforms use symlink, so no input dependencies
		tup_dep_no_exist . foo.txt . "$(tup_ln_cmd foo.txt foo.lnk)"
		tup_dep_no_exist . bar.txt . "$(tup_ln_cmd bar.txt bar.lnk)"

		tup_dep_no_exist sub baz.txt sub "$(tup_ln_cmd baz.txt baz.lnk)"
		tup_dep_no_exist sub blah.txt sub "$(tup_ln_cmd blah.txt blah.lnk)"
		;;
esac

eotup
