#! /bin/sh -e

nums=`seq 1 $1`
echo $nums | xargs tup touch
echo $nums | xargs tup node_exists
if tup node_exists 1 $1; then
	:
else
	exit 1
fi
