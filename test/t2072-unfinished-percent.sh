#! /bin/sh -e

# Make sure a % at the end of a command string results in an error.

. ../tup.sh
cat > Tupfile << HERE
: |> echo % |>
HERE
tup touch Tupfile
update_fail_msg "Unfinished %-flag"
