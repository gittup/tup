#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# Try extra-outputs with a !-macro. The shell script mimics a Windows linker.

. ./tup.sh

echo 'out=$1; base=`basename $out .dll`; shift; cat $* > $out; touch $base.lib $base.exp' > ok.sh
chmod +x ok.sh

cat > Tupfile << HERE
!cc = |> gcc -c %f -o %o |> %B.o
!ld = |> ./ok.sh %o %f |> | %O.lib %O.exp

: foreach *.c |> !cc |> {objs}
: {objs} |> !ld |> out.dll
HERE
touch Tupfile foo.c bar.c ok.sh
update

eotup
