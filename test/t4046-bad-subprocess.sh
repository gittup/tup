#! /bin/sh -e

# Make sure we can't run certain commands as a sub-process.
. ./tup.sh

cat > Tupfile << HERE
: |> tup touch foo |>
HERE
tup touch Tupfile
update_fail_msg "Command 'touch' is not valid when running as a sub-process"

eotup
