#! /bin/sh -e

tup touch foo.c
if [ ! -d ".tup/object/e5/5780e6340b6e110e51e79f077052cb086292a3" ]; then
	echo "Object not created!" 1>&2
	exit 1
fi

if [ ! -f ".tup/create/e55780e6340b6e110e51e79f077052cb086292a3" ]; then
	echo "Create link not established!" 1>&2
	exit 1
fi

# 2 files: .name and .secondary
if find .tup/object/ -type f | wc -l | grep 2 > /dev/null; then
	:
else
	echo "Only one object expected in .tup/object/ !" 1>&2
	exit 1
fi
