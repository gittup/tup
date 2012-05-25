#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2012  Mike Shal <marfey@gmail.com>
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

# I had a bug in the reclaim_ghosts() function since it wouldn't remove the
# tup_entrys when deleting nodes. It was tricky to trigger, since you had to
# move a directory out of tup that contained ghost nodes, so those nodes would
# only be deleted in reclaim_ghosts() rather than in the recursion that happens
# when deleting a directory. Then if you created new nodes, they might have the
# same tupids as the old ghosts, which still existed as tup_entrys in the
# monitor. This confused the monitor, and would cause errors about the tupid
# already being in the tree. This test case reproduces the behavior of having
# some ghost nodes in a directory that gets moved away, then new nodes are
# created to have the same tupids.

. ./tup.sh
check_monitor_supported
mkdir tuptest
cd tuptest
re_init
monitor

mkdir -p foo
cd foo
(echo '#include "secret/ghost.h"'; echo 'int main(void) {return 0;}') > foo.c
echo ': foreach *.c |> gcc -I. -I.. %f -o %o |> %B' > Tupfile
cd ..
mkdir secret
touch secret/ghost.h
update

signal_monitor

mv foo ..
tup flush
signal_monitor

mkdir -p bizzle/nibble/fiddle
cd bizzle/nibble/fiddle
echo 'int main(void) {return 0;}' > ok.c
echo ': foreach *.c |> gcc %f -o %o |> %B' > Tupfile
cd ../../..
update

stop_monitor

eotup
