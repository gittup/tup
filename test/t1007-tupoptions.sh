#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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
	if tup options $3 | grep "$1.*$2" > /dev/null; then
		:
	else
		echo "Error: Expected option value $1 to be set to $2" 1>&2
		exit 1
	fi
}

. ./tup.sh
check_no_windows HOME environment variable

# Override HOME and XDG_CONFIG_HOME so we can control
# the location of the options files.

# Test ${XDG_CONFIG_HOME:-$HOME/.config}/tup/options
export HOME=`pwd`
export XDG_CONFIG_HOME=
check keep_going 0

mkdir -p "$HOME/.config/tup"
cat > .config/tup/options << HERE
[updater]
num_jobs = 4
keep_going = 1
HERE
check num_jobs 4
check keep_going 1

export XDG_CONFIG_HOME="$HOME/.alt_config"
check keep_going 0

mkdir -p "$XDG_CONFIG_HOME/tup"
cat > "$XDG_CONFIG_HOME/tup/options" << HERE
[updater]
num_jobs = 5
HERE
check num_jobs 5

# Test ~/.tupoptions, which overrides the previous file.
cat > .tupoptions << HERE
[updater]
num_jobs = 2
HERE
check num_jobs 2
check keep_going 0

cat > .tupoptions << HERE
[updater]
num_jobs = 3
keep_going = 1
HERE
check num_jobs 3
check keep_going 1

check num_jobs 4 -j4

eotup
