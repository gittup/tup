#! /bin/sh -e

mkdir foo
cd foo
tup touch bar.c
cd ..
if [ ! -d ".tup/object/58/118522f6558c5e1fcc10312dc6a55edef578ca" ]; then
	echo "Object not created!" 1>&2
	exit 1
fi
