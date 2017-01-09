#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2017  Mike Shal <marfey@gmail.com>
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

# Make sure a graph with tup.config changed doesn't graph everything that
# uses @-variables.
. ./tup.sh

varsetall FOO=y
tmkdir sub
cat > sub/Tupfile << HERE
ifeq (@(FOO),1)
: |> echo foo |>
endif
HERE
update

tup touch tup.config

tup graph --dirs > ok.dot
gitignore_good tup.config ok.dot
gitignore_bad FOO ok.dot
gitignore_bad sub ok.dot

eotup
