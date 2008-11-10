#! /bin/sh -e

cp ../testMakefile ./Makefile
nums=`seq 1 $1`
for i in $nums; do echo "void foo$i(void) {}" > $i.c; tup touch $i.c; done
echo "int main(void) {}" >> 1.c
tup upd
if nm prog | grep main > /dev/null; then
	:
else
	echo "Main program not built!" 1>&2
	exit 1
fi
for i in $nums; do tup delete $i.c; rm -f $i.c; done
tup upd
if [ -f prog ]; then
	echo "Main program not deleted!" 1>&2
	exit 1
fi
