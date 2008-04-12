#! /bin/sh -e

for i in `seq 1 $1`; do tup input $i foo; done
