#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Test out ~/.tupoptions
check()
{
	if tup options | grep "$1.*$2" > /dev/null; then
		:
	else
		echo "Error: Expected option value $1 to be set to $2" 1>&2
		exit 1
	fi
}

. ./tup.sh
# Override HOME so we can control ~/.tupoptions
export HOME=`pwd`
re_init
check keep_going 0

cat > .tupoptions << HERE
[updater]
num_jobs = 2
HERE
re_init
check num_jobs 2
check keep_going 0

cat > .tupoptions << HERE
[updater]
num_jobs = 3
keep_going = 1
HERE
re_init
check num_jobs 3
check keep_going 1

eotup
