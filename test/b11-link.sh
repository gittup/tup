#! /bin/sh -e

for i in `seq 1 $1`; do tup touch $i $i.o; tup link "foo" -i$i -o$i.o; done
