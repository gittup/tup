#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2016  Mike Shal <marfey@gmail.com>
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

# Make sure getcwd() returns a real-looking directory, not a .tup/mnt
# directory.

. ./tup.sh
check_no_windows paths
check_tup_suid

cat > foo.c << HERE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	char *pwd;
	char cwd[4096];
	pwd = getenv("PWD");
	printf("Cwd: %s\n", getcwd(cwd, sizeof(cwd)));
	printf("PWD: %s\n", pwd);
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
: foo |> ^c^ ./foo > %o |> output.txt
HERE
tup touch foo.c Tupfile
update

if ! grep "Cwd: $PWD$" output.txt > /dev/null; then
	echo "Error: Expected getcwd() to return current directory." 1>&2
	exit 1
fi
if ! grep "PWD: $PWD$" output.txt > /dev/null; then
	echo "Error: Expected getcwd() to return current directory." 1>&2
	exit 1
fi

eotup
