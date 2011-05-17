#! /bin/sh -e

# We have a ghost node that is pointed to by another node, then move a
# directory over the parent ghost. The symlink's tupid should be updated.
. ./tup.sh
check_monitor_supported
tup monitor

ln -s secret/ghost a
if ! tup graph | grep 'node_5 -> node_3'; then
	echo "Error: Symlink points to the wrong place" 1>&2
	exit 1
fi

mkdir foo
touch foo/ghost

if ! tup graph | grep 'node_5 -> node_3'; then
	echo "Error: Symlink points to the wrong place" 1>&2
	exit 1
fi
mv foo secret

if ! tup graph | grep 'node_7 -> node_3'; then
	echo "Error: Symlink was not updated" 1>&2
	exit 1
fi

eotup
