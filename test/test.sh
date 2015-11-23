#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2015  Mike Shal <marfey@gmail.com>
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

# Use as:
# To run a single test: ./test.sh t0000-init.sh
# To run all tests: ./test.sh
# To run all tests without stopping on errors: ./test.sh --keep-going
#
# You can also invoke a test case directly (as in: ./t0000-init.sh), but if it
# fails you may not always get an error message. You can still tell if the test
# failed by checking the return code (non-zero), and the tuptesttmp directory
# for that test will remain for manual inspection.

keep_going=0
while [ $# -gt 0 ]; do
	if [ "$1" = "-k" ]; then
		keep_going=1
	elif [ "$1" = "--keep-going" ]; then
		keep_going=1
	else
		files="$files $1"
	fi
	shift
done

if [ "$files" = "" ]; then
	files="t[0-9]*.sh"
fi

export tupos=`uname -s`

declare -A failed_tests
for i in $files; do
	echo "[36m --- Run $i --- [0m"
	if ./$i; then
		:
	else
		failed_tests[$i]=$?
		echo "[31m *** $i failed[0m" >&2
		if [ "$keep_going" = "0" ]; then
			exit 1
		fi
	fi
done

n_failed=${#failed_tests[@]}
if [ $n_failed -ge 1 ] ; then
    echo -e "\e[33m$n_failed test(s) failed\e[0m:" >&2
    for t in "${!failed_tests[@]}" ; do
        echo -e "    \e[31m$t\e[0m  exit code ${failed_tests[$t]}" >&2
    done
    exit 1
fi
