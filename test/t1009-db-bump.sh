#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2017-2022  Mike Shal <marfey@gmail.com>
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

. ./tup.sh
check_no_windows sqlite3 executable

# Re-create the tup database from DB_VERSION 1
rm -rf .tup
mkdir .tup
touch .tup/monitor
touch .tup/object
touch .tup/updater
sqlite3 .tup/db << HERE
create table node (id integer primary key not null, dir integer not null, type integer not null, flags integer not null, name varchar(4096));
create table link (from_id integer, to_id integer);
create table var (id integer primary key not null, value varchar(4096));
create table config(lval varchar(256) unique, rval varchar(256));
create index node_dir_index on node(dir, name);
create index node_flags_index on node(flags);
create index link_index on link(from_id);
create index link_index2 on link(to_id);
insert into config values('show_progress', 1);
insert into config values('keep_going', 0);
insert into config values('db_sync', 1);
insert into config values('db_version', 0);
insert into node values(1, 0, 2, 2, '.');
insert into node values(2, 1, 2, 2, '@');
update config set rval=0 where lval='db_sync';
update config set rval=1 where lval='db_version';
HERE
cat > .tup/options << HERE
[db]
sync=0
HERE

cp ../testTupfile.tup Tupfile
echo "int main(void) {}" > foo.c
update
sym_check foo.o main

eotup
