#! /bin/sh -e

# See that a failed command in a string of multiple commands fails. It's rather
# silly to be force to do && between everything. Plus in the example below,
# making the seemingly innocuous change of removing 'echo bar' would cause the
# command to now break if we didn't do this.

. ./tup.sh
cat > Tupfile << HERE
: |> echo foo; false; echo bar |>
HERE

tup touch Tupfile
update_fail

eotup
