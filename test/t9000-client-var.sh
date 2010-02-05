#! /bin/sh -e

# Simple test to verify that a client program can try to access variables,
# causing those variables to become ghosts.

. ./tup.sh

make_tup_client

cat > Tupfile << HERE
: |> ./client abcd |>
HERE
tup touch Tupfile
update

tup_object_exist @ abcd

cat > Tupfile << HERE
: |> ./client defg |>
HERE
tup touch Tupfile
update

tup_object_no_exist @ abcd
tup_object_exist @ defg

eotup
