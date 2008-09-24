#! /bin/sh -e

for i in `seq 1 $1`; do tup touch $i $i.o; create_dep "foo" -i$i -o$i.o; done
