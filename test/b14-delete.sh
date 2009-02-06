#! /bin/sh -e

cp ../testTupfile.tup ./Tupfile
for i in `seq 1 $1`; do echo "void foo$i(void) {}" > $i.c; done
seq 1 $1 | sed 's/$/.c/' | xargs tup touch
echo "int main(void) {}" >> 1.c
tup upd
if nm prog | grep main > /dev/null; then
	:
else
	echo "Main program not built!" 1>&2
	exit 1
fi
seq 1 $1 | sed 's/$/.c/' | xargs rm -f
seq 1 $1 | sed 's/$/.c/' | xargs tup delete
tup upd
if [ -f prog ]; then
	echo "Main program not deleted!" 1>&2
	exit 1
fi
