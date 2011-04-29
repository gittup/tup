#! /bin/sh -e

# Try utimens.

. ./tup.sh

cat > Tupfile << HERE
: |> echo hey > %o; touch -t 202005080000 %o |> foo
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> touch -t 202005080000 test2 |>
HERE
tup touch Tupfile test2
update_fail_msg "tup error.*utimens"

eotup
