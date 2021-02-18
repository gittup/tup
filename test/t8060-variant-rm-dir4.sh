#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# If we delete the variant directory as a result of the srctree going away, but
# then fail to parse, the fact that we removed those directory nodes is not
# committed to the database. As a result, the next scan calls tup_file_missing
# on the variant src directories, resulting in the srctree going away and
# making the srcid on the variant nodes invalid. Since the variant nodes are
# ghosts, they should be reset to a srcid of -1. Note this means there is a
# slightly different error message - in one case we can't find the 'sub'
# directory, and in the next case we can find it, but since it is a ghost we
# can't find the file in it.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir build-debug

tmkdir sub
cat > sub/Tupfile << HERE
: |> echo "generated" > %o |> gen
HERE
tmkdir foo
cat > foo/Tupfile << HERE
: ../sub/gen |> cat %f > %o |> output.txt
HERE

tmkdir foo2
cat > foo2/Tupfile << HERE
: |> if [ -f ../sub/normal ]; then cat ../sub/normal; else echo nofile; fi > %o |> output.txt
HERE

echo "" > build/tup.config
echo "CONFIG_DEBUG=y" > build-debug/tup.config
echo "normal" > sub/normal
tup touch build/tup.config build-debug/tup.config sub/Tupfile foo/Tupfile foo2/Tupfile

update

echo "generated" | diff - build/foo/output.txt
echo "generated" | diff - build-debug/foo/output.txt
echo "normal" | diff - build/foo2/output.txt
echo "normal" | diff - build-debug/foo2/output.txt

rm -rf sub
update_fail_msg "Failed to find directory ID for dir '../sub/gen' relative to 'build-debug/foo'"

# Make sure that if we try to re-parse the Tupfile we still get the same error message.
tup touch foo/Tupfile
update_fail_msg "Explicitly named file 'gen' not found"

rm foo/Tupfile
tup rm foo/Tupfile
update

check_not_exist build/foo/output.txt
check_not_exist build-debug/foo/output.txt
echo "nofile" | diff - build/foo2/output.txt
echo "nofile" | diff - build-debug/foo2/output.txt

eotup
