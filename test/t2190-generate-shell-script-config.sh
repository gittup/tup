#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2021  Mike Shal <marfey@gmail.com>
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

# 'tup generate' runs without a tup directory
rm -rf .tup

mkdir sub1
mkdir sub2
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
mkdir configs
echo 'CONFIG_FOO=1' > configs/foo.config

generate $generate_script_name
gitignore_good 'echo bar' $generate_script_name
gitignore_bad 'echo foo' $generate_script_name
gitignore_bad 'echo baz' $generate_script_name

generate $generate_script_name --config configs/foo.config
gitignore_bad 'echo bar' $generate_script_name
gitignore_good 'echo foo' $generate_script_name
gitignore_bad 'echo baz' $generate_script_name

echo 'CONFIG_BAZ=1' > tup.config
generate $generate_script_name
gitignore_good 'echo bar' $generate_script_name
gitignore_bad 'echo foo' $generate_script_name
gitignore_good 'echo baz' $generate_script_name

eotup
