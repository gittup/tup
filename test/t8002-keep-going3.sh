#! /bin/sh -e

# Make sure keep-going will fail if we have a command with no outputs

. ./tup.sh

cat > Tupfile << HERE
: |> echo foo |>
: |> echo bar; exit 1 |>
: |> echo baz |>
HERE

update_fail -k

eotup
