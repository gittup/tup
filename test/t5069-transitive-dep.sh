#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
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

# Here we have two shared libraries, where libbar.so is dependent on libfoo.so
# Also we have a main program that is dependent on libbar.so, but doesn't know
# about libfoo.so, so it would be nice to not have to specify that dependency.
# This only works if the graph from libfoo.so to the program is "strongly 
# connected", which means that the links are both normal and sticky all the
# way through. If we allowed only sticky links, then the graph truncation that
# occurs for nodes that are only pointed to by sticky links may allow you to
# sneak an illegal dependency into the graph. (eg: a s> b -> c and have it build
# correctly, then remove the 'a s> b' link and update. Node 'c' wouldn't
# necessarily get rebuilt because the graph would be truncated). By requiring
# a strongly-connected graph, we can be sure that any nodes that use
# inherited dependencies will be updated again, and therefore will re-check
# that the links are valid.
#
# Note that this means we can't also skip the parts of the DAG if the output
# ends up being the same as a previous run, since it would potentially
# violate the assumptions in the "strongly connected" graph.

. ./tup.sh
check_no_windows shlib

cat > foo.c << HERE
#include "foo.h"
int foo(void) {return 5;}
HERE
echo 'int foo(void);' > foo.h

cat > bar.c << HERE
#include "bar.h"
#include "foo.h"
int bar(void) {return foo() * 8;}
HERE
echo 'int bar(void);' > bar.h

cat > main.c << HERE
#include "bar.h"
int main(void)
{
	if(bar() != 40)
		return 1;
	return 0;
}
HERE

cat > Tupfile << HERE
: foreach *.c |> gcc -fpic -c %f -o %o |> %B.o
: foo.o |> gcc -fpic -shared %f -o %o |> libfoo.so
: bar.o | libfoo.so |> gcc -fpic -shared %f -o %o -L. -lfoo |> libbar.so
: main.o | libbar.so |> LD_LIBRARY_PATH=. gcc main.o -o %o -L. -lbar |> prog
: prog |> LD_LIBRARY_PATH=. ./prog |>
HERE
update

eotup
