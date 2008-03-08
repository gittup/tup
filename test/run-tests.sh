#! /bin/sh
for i in 1 10 100 1000 10000; do ./run-test.sh -n $i > out-$i.txt 2>&1; done
