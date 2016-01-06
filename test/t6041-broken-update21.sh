#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2016  Mike Shal <marfey@gmail.com>
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

# Tup is supposed to handle files that are both read from and written to as
# if they are only written to. That way the command points to the written-to
# file, but the file can't point back to the command (thus causing a circular
# dependency). Since the output files are removed before the command executes,
# there's no point in trying to read from the files anyway (but programs may
# try to stat the file or something before writing to it). However, I was only
# removing things from the read list if they were in the write list. Which
# means if a command didn't actually write the file, it wouldn't be pruned.
# The link would still exist from the parser, and would create a circular
# dependency in the DAG. This tests for that case by trying to read from the
# file that it claims to create. The first update fails because the output
# file isn't created. At this point, when the file checking was incorrect,
# the output file would point back to the command. A second update would then
# fail with a circular dependency.

. ./tup.sh
check_no_windows shell

cat > Tupfile <<HERE
: |> if [ -f output ]; then cat output; fi |> output
HERE
tup touch Tupfile
update_fail_msg "Expected to write to file 'output'"

tup_dep_no_exist . output . 'if [ -f output ]; then cat output; fi'
tup_sticky_no_exist . output . 'if [ -f output ]; then cat output; fi'

update_fail_msg "Expected to write to file 'output'"

eotup
