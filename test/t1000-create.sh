#! /bin/sh -e

. ../tup.sh
tup touch foo.c
tup_object_exist foo.c
tup_create_exist .

# 2 files: .name for foo.c, plus .name for "."
if find .tup/object/ -type f | wc -l | grep 2 > /dev/null; then
	:
else
	echo "Only one object expected in .tup/object/ !" 1>&2
	exit 1
fi
