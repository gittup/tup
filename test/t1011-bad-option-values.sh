#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021  Mike Shal <marfey@gmail.com>
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

# Make sure we get an error message if there's a bad value for various options.
. ./tup.sh

cat > .tup/options << HERE
[display]
color = false
HERE
update_fail_msg "Invalid value 'false' for option 'display.color' - expected one of {never|always|auto}"

cat > .tup/options << HERE
[updater]
num_jobs = yes
HERE
update_fail_msg "Invalid value 'yes' for option 'updater.num_jobs' - expected non-negative number (0, 1, 2, etc)"

cat > .tup/options << HERE
[updater]
keep_going = 3
HERE
update_fail_msg "Invalid value '3' for option 'updater.keep_going' - expected a boolean value {0|false|no|1|true|yes}"

# Make sure all valid values work.
cat > .tup/options << HERE
[display]
color = never
progress = 1
job_numbers = yes
job_time = no

[updater]
num_jobs = 3
keep_going = 0
full_deps = false
warnings = true
HERE
update

eotup
