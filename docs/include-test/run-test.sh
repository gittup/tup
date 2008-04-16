#! /bin/sh
if [ $# -lt 1 ]; then
	echo "Usage: $0 size" 1>&2
	exit 1
fi
rm -f *.c *.d
for i in `seq 1 $1`; do
	touch $i.c
	echo "all: $i.c" > $i.d
done
