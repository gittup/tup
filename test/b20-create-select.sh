#! /bin/sh -e

seq 1 $1 | xargs tup touch
seq 1 $1 | xargs tup node_exists
if tup node_exists 1 $1; then
	:
else
	echo "Node 1 doesn't exist!" 1>&2
	exit 1
fi
