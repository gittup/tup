#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Execute a run-script without a newline at the end of a rule.

. ./tup.sh
check_no_windows run-script shell
check_bash

# Need bash for 'echo -n'
cat > gen.sh << HERE
#! /usr/bin/env bash
echo -n ": \$@ |> do_cmd |> "
for i in \$@; do
	echo -n \`basename \$i .in\`.out
done
HERE
chmod +x gen.sh

cat > Tupfile << HERE
run ./gen.sh
HERE
touch a.in b.in c.in
parse_fail_msg "Missing newline from :-rule in run script"

eotup
