#! /bin/sh
# tup - A file-based build system
#
# Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
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

tupcurdir=$PWD

# Prefix PATH so the test cases run the local tup
PATH=$PWD/..:$PATH
export PATH

testname=`echo $0 | sed 's/\.\///; s/\.sh//'`
tuptestdir="tuptesttmp-$testname"

# tupos may be set already by test.sh - this is an optimization to
# avoid extra forks in win32
: ${tupos:=`uname -s`}

rm -rf $tuptestdir
mkdir $tuptestdir
cd $tuptestdir
tup init --no-sync --force

tupcp='cp'
case $tupos in
CYGWIN*)
       # Avoid problems with CR/LF vs LF
       alias diff='diff -b'
       in_windows=1
       tupcp='tup-cp'
;;
esac

tmkdir()
{
	mkdir $1
	tup touch $1
}

symeotup()
{
	echo "[33m'touch -h' is not supported - skipping test.[0m" 1>&2
	eotup
}

symtouch()
{
# Some machines may not support 'touch -h' to change the timestamp on the
# symlink. In these cases, just quit the test.
	touch -h $@ || symeotup
}

check_empty_tupdirs()
{
	if tup flags_exists; then
		:
	else
		echo "*** Nodes shouldn't have flags set" 1>&2
		exit 1
	fi
}

sym_check()
{
	f=$1
	shift
	if [ ! -f $f ]; then
		echo "*** Object file does not exist: $f" 1>&2
		exit 1
	fi
	while [ $# -gt 0 ]; do
		sym=$1
		if echo $sym | grep '^^' > /dev/null; then
			sym=`echo $sym | sed 's/^^//'`
			if nm $f | grep $sym > /dev/null; then
				echo "*** '$sym' shouldn't exist in '$f'" 1>&2
				exit 1
			fi
		else
			if nm $f | grep $sym > /dev/null; then
				:
			else
				echo "*** No symbol '$sym' in object '$f'" 1>&2
				exit 1
			fi
		fi
		shift
	done
}

check_exist()
{
	while [ $# -gt 0 ]; do
		if [ ! -f $1 ]; then
			echo "*** File '$1' does not exist when it should" 1>&2
			exit 1
		fi
		shift
	done
}

check_not_exist()
{
	while [ $# -gt 0 ]; do
		if [ -f $1 ]; then
			echo "*** File '$1' exists when it shouldn't" 1>&2
			exit 1
		fi
		shift
	done
}

tup_object_exist()
{
	dir=$1
	shift
	if [ $# -le 0 ]; then
		echo "*** tup_object_exist needs a dir and files" 1>&2
		exit 1
	fi
	while [ $# -gt 0 ]; do
		if tup node_exists $dir "$1"; then
			:
		else
			echo "*** Missing node \"$1\" from .tup/db" 1>&2
			exit 1
		fi
		shift
	done
}

tup_object_no_exist()
{
	dir=$1
	shift
	if [ $# -le 0 ]; then
		echo "*** tup_object_no_exist needs a dir and files" 1>&2
		exit 1
	fi
	while [ $# -gt 0 ]; do
		# stderr redirection is to ignore warnings about non-existant
		# directories when checking for ghost files
		if tup node_exists $dir "$1" 2>/dev/null; then
			echo "*** Node \"$1\" exists in .tup/db when it shouldn't" 1>&2
			exit 1
		fi
		shift
	done
}

tup_dep_exist()
{
	if tup link_exists "$1" "$2" "$3" "$4"; then
		:
	else
		echo "*** Dependency from $2 [$1] -> $4 [$3] does not exist" 1>&2
		exit 1
	fi
}

tup_dep_no_exist()
{
	if tup node_exists "$1" "$2"; then
		if tup node_exists "$3" "$4"; then
			if tup link_exists "$1" "$2" "$3" "$4"; then
				echo "*** Dependency from $2 [$1] -> $4 [$3] exists when it shouldn't" 1>&2
				exit 1
			fi
		else
			echo "*** Object $4 [$3] does not exist" 1>&2
			exit 1;
		fi
	else
		echo "*** Object $2 [$1] does not exist" 1>&2
		exit 1;
	fi
}

__update()
{
	if [ -n "$TUP_VALGRIND" ]; then
		cmd="valgrind -q --error-exitcode=11 --sim-hints=fuse-compatible --track-fds=yes --track-origins=yes --leak-check=full tup upd"
	elif [ -n "$TUP_HELGRIND" ]; then
		cmd="valgrind -q --error-exitcode=12 --sim-hints=fuse-compatible --tool=helgrind tup upd"
	else
		cmd="tup upd"
	fi

	if $cmd "$@"; then
		:
	else
		echo "*** Failed to update!" 1>&2
		exit 1
	fi
}

update()
{
	__update $@
	check_empty_tupdirs
}

update_partial()
{
	__update $@
}

update_fail()
{
	if tup upd "$@" 2>/dev/null; then
		echo "*** Expected update to fail, but didn't" 1>&2
		exit 1
	else
		echo "Update expected to fail, and did"
	fi
}

update_fail_msg()
{
	if tup upd 2>.tupoutput; then
		echo "*** Expected update to fail, but didn't" 1>&2
		exit 1
	else
		while [ $# -gt 0 ]; do
			if grep "$1" .tupoutput > /dev/null; then
				echo "Update expected to fail, and failed for the right reason."
			else
				echo "*** Update expected to fail because of: $1" 1>&2
				echo "*** But failed because of:" 1>&2
				cat .tupoutput 1>&2
				exit 1
			fi
			shift
		done
	fi
}

parse_fail_msg()
{
	if tup parse 2>.tupoutput; then
		echo "*** Expected parsing to fail, but didn't" 1>&2
		exit 1
	else
		if grep "$1" .tupoutput > /dev/null; then
			echo "Parsing expected to fail, and failed for the right reason."
		else
			echo "*** Parsing expected to fail because of: $1" 1>&2
			echo "*** But failed because of:" 1>&2
			cat .tupoutput 1>&2
			exit 1
		fi
	fi
}

check_same_link()
{
	if stat $* | grep Inode | awk 'BEGIN{x=-1} {if(x == -1) {x=$4} if(x != $4) {exit 1}}'; then
		:
	else
		echo "*** Files '$*' are not the same inode." 1>&2
		exit 1
	fi
}

check_updates()
{
	if [ ! -f $2 ]; then
		echo "check_updates($1, $2) failed because $2 doesn't already exist"
		exit 1
	fi
	rm $2
	tup touch $1
	update --no-scan
	if [ ! -f $2 ]; then
		echo "check_updates($1, $2) failed to re-create $2"
		exit 1
	fi
}

check_no_updates()
{
	if [ ! -f $2 ]; then
		echo "check_no_updates($1, $2) failed because $2 doesn't already exist"
		exit 1
	fi
	mv $2 $2_check_no_updates.bak
	tup touch $1
	update --no-scan
	if [ -f $2 ]; then
		echo "check_no_updates($1, $2) re-created $2 when it shouldn't have"
		exit 1
	fi
	mv $2_check_no_updates.bak $2
}

gitignore_bad()
{
	if grep $1 $2 > /dev/null; then
		echo "Error: $1 found in $2" 1>&2
		exit 1
	fi
}

gitignore_good()
{
	if grep $1 $2 > /dev/null; then
		:
	else
		echo "Error: $1 not found in $2" 1>&2
		exit 1
	fi
}

vardict_exist()
{
	if grep "\<$1\>" .tup/vardict > /dev/null; then
		:
	else
		echo "Error: $1 not found in vardict file" 1>&2
		exit 1
	fi
}

vardict_no_exist()
{
	if grep "\<$1\>" .tup/vardict > /dev/null; then
		echo "Error: $1 found in vardict file when it shouldn't" 1>&2
		exit 1
	fi
}

varsetall()
{
	rm -f tup.config
	while [ $# -gt 0 ]; do
		if echo "$1" | grep "=n$" > /dev/null; then
			var=`echo "$1" | sed 's/=.*//'`
			echo "# CONFIG_$var is not set" >> tup.config
		else
			echo CONFIG_$1 >> tup.config
		fi
		shift
	done
	tup touch tup.config
}

stop_monitor()
{
	tup flush
	if tup stop; then :; else
		echo "Error: tup monitor no longer running when it should be" 1>&2
		exit 1
	fi
}

signal_monitor()
{
	if [ -f .tup/monitor.pid ]; then
		# It's really confusing if you happen to 'kill -USR1 -1', cuz
		# everything disappears.
		text=`cat .tup/monitor.pid`
		if echo "$text" | grep '\-1' > /dev/null; then
			echo "Error: Monitor is not running - unable to signal" 1>&2
			exit 1
		fi

		kill -USR1 $text
	else
		echo "Error: No monitor.pid file running - unable to signal" 1>&2
		exit 1
	fi
}

re_init()
{
	rm -rf .tup
	tup init --no-sync --force
}

make_tup_client()
{
	cat > client.c << HERE
#include "../../tup_client.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	const char *value;
	if(tup_vardict_init() < 0)
		return 1;
	while(argc > 1) {
		value = tup_config_var(argv[1], -1);
		if(value)
			printf("%s\n", value);
		argc--;
		argv++;
	}
	return 0;
}
HERE

	plat_ldflags=""
	if [ "$tupos" = "SunOS" ]; then
		plat_ldflags="$plat_ldflags -lsocket"
	fi
	gcc client.c ../../libtup_client.a -o client $plat_ldflags -ldl
	tup touch client
}

check_monitor_supported()
{
	if tup monitor_supported; then
		:
	else
		echo "Monitor is not supported. Skipping test."
		eotup
	fi
}

check_no_windows()
{
	case $tupos in
	CYGWIN*)
		echo "Not supported in Windows. Skipping test."
		eotup
		;;
	MINGW*)
		echo "Not supported in Windows. Skipping test."
		eotup
		;;
	esac
}

single_threaded()
{
	(echo "[updater]"; echo "num_jobs=1") >> .tup/options
}

set_autoupdate()
{
	(echo "[monitor]"; echo "autoupdate=1") >> .tup/options
}

eotup()
{
	tup stop > /dev/null || true
	cd $tupcurdir
	rm -rf $tuptestdir
	exit 0
}
