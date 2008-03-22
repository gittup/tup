#! /bin/sh -e

nums=`seq 1 $1`
for i in $nums; do echo "void foo$i(void) {}" > $i.c; tup touch $i.c; done
tup update
for i in $nums; do tup touch $i.c; done
tup update
