#! /bin/sh -e

# See if we can escape an at-sign

. ../tup.sh
cat > Tupfile << HERE
: |> echo "\@hey" |>
: |> echo "\@(hey" |>
HERE
tup touch Tupfile
update

tup_object_exist . 'echo "\@hey"'
tup_object_exist . 'echo "@(hey"'
