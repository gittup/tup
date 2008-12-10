#! /bin/sh -e

seq 1 $1 | xargs tup touch
seq 1 $1 | sed 's/$/.o/' | xargs tup touch
tup node_exists . 1 1.o 50 50.o 100 100.o
for i in `seq 1 $1`; do tup link "foo" -i$i -o$i.o; done
