#! /bin/sh -e

cp ../testMakefile ./Makefile
nums=`seq 1 $1`
for i in $nums; do echo "void foo$i(void) {}" > $i.c; tup touch $i.c; done
echo "int main(void) {}" >> 1.c
tup update
for i in $nums; do tup delete $i.c; done
tup update
