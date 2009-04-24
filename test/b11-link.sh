#! /bin/sh -e

seq 1 $1 | xargs tup node
seq 1 $1 | sed 's/$/.o/' | xargs tup node
tup node_exists . 1 1.o 50 50.o 100 100.o
seq 1 $1 | sed 's/^/-i/' | xargs tup link "foo"
seq 1 $1 | sed 's/^/-o/; s/$/.o/' | xargs tup link "foo"
tup link_exists . 1 . foo
tup link_exists . foo . 1.o
