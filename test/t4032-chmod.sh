#! /bin/sh -e

# Try chmod.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o; chmod 664 %o |> test1
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> chmod 664 test2 |>
HERE
tup touch Tupfile test2
update_fail_msg "tup error.*chmod"

eotup
