#! /bin/sh
# tup - A file-based build system
#
# Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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

generate_script_name="build.sh"
case $tupos in
CYGWIN*)
	# Avoid problems with CR/LF vs LF
	alias diff='diff -b'
	in_windows=1
	generate_script_name="build.bat"
;;
esac

# override any user settings for grep
GREP_OPTIONS=""
export GREP_OPTIONS

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
		if [ ! -e $1 ]; then
			echo "*** File '$1' does not exist when it should" 1>&2
			exit 1
		fi
		shift
	done
}

check_not_exist()
{
	while [ $# -gt 0 ]; do
		if [ -e $1 ]; then
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

dep_exist_internal()
{
	link_type=$1
	shift
	set +e
	tup ${link_type}_exists "$1" "$2" "$3" "$4"
	rc=$?
	set -e
	if [ "$rc" = "11" ]; then
		:
	elif [ "$rc" = "0" ]; then
		echo "*** Dependency from $2 [$1] -> $4 [$3] does not exist" 1>&2
		exit 1
	else
		echo "*** link_exists() failed." 1>&2
		exit 1
	fi
}

dep_no_exist_internal()
{
	link_type=$1
	shift
	if tup node_exists "$1" "$2"; then
		if tup node_exists "$3" "$4"; then
			set +e
			tup ${link_type}_exists "$1" "$2" "$3" "$4"
			rc=$?
			set -e
			if [ "$rc" = "11" ]; then
				echo "*** Dependency from $2 [$1] -> $4 [$3] exists when it shouldn't" 1>&2
				exit 1
			elif [ "$rc" = "0" ]; then
				:
			else
				echo "*** link_exists() failed." 1>&2
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

tup_dep_exist()
{
	dep_exist_internal normal "$1" "$2" "$3" "$4"
}

tup_dep_no_exist()
{
	dep_no_exist_internal normal "$1" "$2" "$3" "$4"
}

tup_sticky_exist()
{
	dep_exist_internal sticky "$1" "$2" "$3" "$4"
}

tup_sticky_no_exist()
{
	dep_no_exist_internal sticky "$1" "$2" "$3" "$4"
}

set_leak_check()
{
	leak_check=$1
}

__update()
{
	if [ `tup server` = "fuse" ]; then
		sim_hints="--sim-hints=fuse-compatible"
	fi
	if [ -n "$TUP_VALGRIND" ]; then
		cmd="valgrind -q --error-exitcode=11 $sim_hints --track-fds=yes --track-origins=yes --leak-check=${leak_check-full} tup"
	elif [ -n "$TUP_HELGRIND" ]; then
		cmd="valgrind -q --error-exitcode=12 $sim_hints --tool=helgrind tup"
	else
		cmd="tup"
	fi

	set +e
	$cmd $@
	rc=$?
	set -e

	if [ "$rc" = "11" ]; then
		echo "*** Failed valgrind!" 1>&2
		exit 1
	fi
	if [ "$rc" = "12" ]; then
		echo "*** Failed helgrind!" 1>&2
		exit 1
	fi

	if [ "$rc" = "0" ]; then
		:
	else
		echo "*** Failed to update!" 1>&2
	fi
	return "$rc"
}

update()
{
	if ! __update $@; then
		exit 1
	fi
	check_empty_tupdirs
}

update_partial()
{
	if ! __update $@; then
		exit 1
	fi
}

update_fail()
{
	set_leak_check no
	if __update $@ 2>/dev/null; then
		echo "*** Expected update to fail, but didn't" 1>&2
		exit 1
	else
		echo "Update expected to fail, and did"
	fi
	set_leak_check full
}

update_fail_msg()
{
	set_leak_check no
	if __update 2>.tup/.tupoutput; then
		echo "*** Expected update to fail, but didn't" 1>&2
		exit 1
	else
		while [ $# -gt 0 ]; do
			if grep "$1" .tup/.tupoutput > /dev/null; then
				echo "Update expected to fail, and failed for the right reason."
			else
				echo "*** Update expected to fail because of: $1" 1>&2
				echo "*** But failed because of:" 1>&2
				cat .tup/.tupoutput 1>&2
				exit 1
			fi
			shift
		done
	fi
	set_leak_check full
}

parse()
{
	if ! __update parse; then
		exit 1
	fi
}

parse_fail_msg()
{
	if tup parse 2>.tup/.tupoutput; then
		echo "*** Expected parsing to fail, but didn't" 1>&2
		exit 1
	else
		if grep "$1" .tup/.tupoutput > /dev/null; then
			echo "Parsing expected to fail, and failed for the right reason."
		else
			echo "*** Parsing expected to fail because of: $1" 1>&2
			echo "*** But failed because of:" 1>&2
			cat .tup/.tupoutput 1>&2
			exit 1
		fi
	fi
}

refactor()
{
	if ! tup refactor; then
		exit 1
	fi
}

refactor_fail_msg()
{
	if tup refactor 2>.tup/.tupoutput; then
		echo "*** Expected refactoring to fail, but didn't" 1>&2
		exit 1
	else
		if grep "$1" .tup/.tupoutput > /dev/null; then
			echo "Refactoring expected to fail, and failed for the right reason."
		else
			echo "*** Refactoring expected to fail because of: $1" 1>&2
			echo "*** But failed because of:" 1>&2
			cat .tup/.tupoutput 1>&2
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
	if grep "$1" $2 > /dev/null; then
		echo "Error: $1 found in $2" 1>&2
		exit 1
	fi
}

gitignore_good()
{
	if grep "$1" $2 > /dev/null; then
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
	if grep "\<$1\>" .tup/vardict > /dev/null 2>/dev/null; then
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

monitor()
{
	if [ -n "$TUP_VALGRIND" ]; then
		cmd="valgrind -q --error-exitcode=11 --sim-hints=fuse-compatible --track-fds=yes --track-origins=yes --leak-check=full tup monitor -f"
	elif [ -n "$TUP_HELGRIND" ]; then
		cmd="valgrind -q --error-exitcode=12 --sim-hints=fuse-compatible --tool=helgrind tup monitor -f"
	else
		cmd="tup monitor -f"
	fi

	if $cmd "$@" & then
		monitor_pid=$!
		# The monitor may not actually have the lock yet - use waitmon to wait
		# until it is started
		tup waitmon
	else
		echo "*** Failed to run the monitor!" 1>&2
		exit 1
	fi
	monitor_running=1
}

wait_monitor()
{
	wait $monitor_pid
	if [ $? != 0 ]; then
		echo "Error: monitor (pid $monitor_pid) exited with error code $?" 1>&2
		exit 1
	fi
	monitor_running=0
}

stop_monitor()
{
	tup flush
	if tup stop; then :; else
		echo "Error: tup monitor no longer running when it should be" 1>&2
		exit 1
	fi
	wait_monitor
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

generate()
{
	if [ -n "$TUP_VALGRIND" ]; then
		cmd="valgrind -q --error-exitcode=11 --sim-hints=fuse-compatible --track-fds=yes --track-origins=yes --leak-check=full tup generate"
	elif [ -n "$TUP_HELGRIND" ]; then
		cmd="valgrind -q --error-exitcode=12 --sim-hints=fuse-compatible --tool=helgrind tup generate"
	else
		cmd="tup generate"
	fi
	$cmd "$@"
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
	if ldd ../../tup | grep 'libasan' > /dev/null; then
		plat_ldflags="$plat_ldflags -lasan -lubsan"
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

check_windows()
{
	case $tupos in
	CYGWIN*)
		return
		;;
	MINGW*)
		return
		;;
	esac
	echo "Only supported in Windows. Skipping test."
	eotup
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

check_no_osx()
{
	case $tupos in
	Darwin*)
		echo "Not supported in OSX. Skipping test."
		eotup
		;;
	esac
}

check_tup_no_suid()
{
	if ! tup privileged; then
		echo "Tup is privileged, but this test requires that it does not. Skipping test."
		eotup
	fi
}

check_tup_suid()
{
	if tup privileged; then
		echo "Tup needs to be privileged for this test to run. Skipping test."
		eotup
	fi
}

check_python()
{
	if ! which python > /dev/null 2>&1; then
		echo "[33mNo python found - skipping test.[0m"
		eotup
	fi
	# Need 2.6 for the -B flag
	cat > tmpversioncheck.py << HERE
import sys
if sys.version_info < (2, 6):
    sys.exit(1)
sys.exit(0)
HERE
	if ! python tmpversioncheck.py; then
		echo "[33mPython < version 2.6 found - skipping test.[0m"
		eotup
	fi
	rm tmpversioncheck.py
}

check_bash()
{
    if [ ! -e /usr/bin/env ]; then
		echo "[33m/usr/bin/env not found - skipping test.[0m"
		eotup
    elif ! which bash > /dev/null 2>&1; then
		echo "[33mNo bash found - skipping test.[0m"
		eotup
    fi
}

single_threaded()
{
	(echo "[updater]"; echo "num_jobs=1") >> .tup/options
}

set_autoupdate()
{
	(echo "[monitor]"; echo "autoupdate=1") >> .tup/options
}

set_full_deps()
{
	(echo "[updater]"; echo "full_deps=1") >> .tup/options
}

clear_full_deps()
{
	(echo "[updater]"; echo "full_deps=0") >> .tup/options
}

tup_ln_cmd() {
	printf "!tup_ln $1 $2"
}

eotup()
{
	if [ "$monitor_running" = "1" ]; then
		stop_monitor
	fi
	cd $tupcurdir
	if [ -f "$tuptestdir/.tup/mnt/.metadata_never_index" ]; then
		rm "$tuptestdir/.tup/mnt/.metadata_never_index"
	fi
	if [ -d "$tuptestdir/.tup/mnt" ]; then
		mntdir=`find $tuptestdir/.tup/mnt -maxdepth 0 -not -empty`
		if [ "$mntdir" != "" ]; then
			echo "Error: $mntdir is not empty yet. Is fuse still mounted?" 1>&2
			exit 1
		fi
	fi
	rm -rf $tuptestdir
	exit 0
}
