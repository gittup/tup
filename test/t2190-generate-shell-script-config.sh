#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2015  Mike Shal <marfey@gmail.com>
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

# Try to generate a shell script using a specific variant config

. ./tup.sh
check_no_windows shell
tmkdir sub1
tmkdir sub2
cat > Tupfile << HERE
ifeq (@(FOO),1)
: |> echo foo |>
else
: |> echo bar |>
endif
ifeq (@(BAZ),1)
: |> echo baz |>
endif
HERE
tmkdir configs
echo 'CONFIG_FOO=1' > configs/foo.config

tup generate build.sh
gitignore_good 'echo bar' build.sh
gitignore_bad 'echo foo' build.sh
gitignore_bad 'echo baz' build.sh

tup generate build.sh --config configs/foo.config
gitignore_bad 'echo bar' build.sh
gitignore_good 'echo foo' build.sh
gitignore_bad 'echo baz' build.sh

echo 'CONFIG_BAZ=1' > tup.config
tup generate build.sh
gitignore_good 'echo bar' build.sh
gitignore_bad 'echo foo' build.sh
gitignore_good 'echo baz' build.sh

eotup
