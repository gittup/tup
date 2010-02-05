#! /bin/sh -e

# Continuation of t6033. So I can't accurately discover circular dependencies
# just by checking which nodes are on plist or not. The problem is if A->B with
# a sticky link, then B won't be expanded immediately. Therefore if somewhere
# down the chain B->...->A, I won't know. Further, if C->B is a normal link,
# then B will be expanded *later*, after A is already finished. The only way
# to detect circular dependencies while building the graph would be to always
# load the entire partial DAG (even nodes that won't be executed, because the
# only incoming links are sticky). Not sure if that would be worthwhile.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c | foo.h |> gcc -c %f -o %o |> foo.o
HERE
tup touch Tupfile foo.c foo.h
update

tup touch foo.h foo.c
update

eotup
