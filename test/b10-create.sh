#! /bin/sh -e

for i in `seq 1 $1`; do tup touch $i; done
