#! /bin/sh -e

# Make sure you can't use 'else' or 'endif' without an if statement.

. ./tup.sh
cat > Tupfile << HERE
: |> echo foo |>
else
: |> echo bar |>
HERE
tup touch Tupfile
parse_fail_msg "else statement outside of an if block"

cat > Tupfile << HERE
: |> echo foo |>
endif
: |> echo bar |>
HERE
tup touch Tupfile
parse_fail_msg "endif statement outside of an if block"

eotup
