#! /bin/sh -e

# Check to make sure we get the warnings from hidden files.
# (This behavior of tup may be stupid).

. ./tup.sh

# The 'exit 1' is just so we can use update_fail_msg. I'm lazy.
cat > Tupfile << HERE
: |> touch .foo; touch .bar; exit 1 |>
HERE
update_fail_msg "Update resulted in 2 warnings"

eotup
