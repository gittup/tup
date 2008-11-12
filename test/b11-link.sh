#! /bin/sh -e

nums=`seq 1 $1`
echo $nums | xargs tup touch
echo $nums | sed 's/$/.o/' | xargs tup touch
for i in $nums; do tup link "foo" -i$i -o$i.o; done
