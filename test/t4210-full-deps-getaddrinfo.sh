#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# When using getaddrinfo, resolv.conf ends up being read. On systemd systems,
# this points to a file in /run, which may change spuriously and cause
# unnecessary rebuilds. It's probably reasonable to generalize this to say we
# don't want to depend on anything in /run or /var/run
. ./tup.sh
check_tup_suid
set_full_deps

cat > Tupfile << HERE
: |> sh run.sh /etc/resolv.conf %o |> out.txt ^/resolv.conf
: |> sh run.sh /run/systemd/resolve/resolv.conf %o |> out2.txt ^/resolv.conf
: |> sh run.sh /var/run/systemd/resolve/resolv.conf %o |> out3.txt ^/resolv.conf
HERE
cat > run.sh << HERE
if [ -f \$1 ]; then cat \$1; else echo nofile; fi > \$2
HERE
update

tup_object_no_exist /etc resolv.conf
tup_object_no_exist /run/systemd/resolve resolv.conf
tup_object_no_exist /var/run/systemd/resolve resolv.conf

eotup
