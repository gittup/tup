#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# Make sure if we ctrl-C tup, that all subprocesses are dead and we still make
# progress on the completed jobs.
. ./tup.sh
check_no_windows process

config()
{
	cat > .tup/options << HERE
[updater]
num_jobs=$1
keep_going=$2
HERE
}

cat > Tupfile << HERE
: |> sh run.sh foo |>
: |> sh run.sh bar |>
: |> sh run.sh baz |>
HERE
cat > run.sh << HERE
echo \$1
exit 1
HERE
config 3 0
update_fail_msg "3 jobs failed"
config 2 0
update_fail_msg "2 jobs failed"
config 1 0
update_fail_msg "1 job failed"

cat > Tupfile << HERE
: |> sh run.sh foo > %o |> out.txt
: out.txt |> sh run.sh bar |>
HERE
config 3 1
update_fail_msg "1 job failed" "Remaining nodes skipped due to errors in command execution"

eotup
