#! /bin/sh -e

# Like t6021, but this time we re-order things so the 'output' file is marked
# as delete when we parse the 'cat output' rule so we don't get yelled at.
# However, the command needs to be re-executed in order to see that the output
# file is no longer necessary (since here, it is).

. ../tup.sh
cat > Tupfile << HERE
: |> echo foo > %o |> output
: output |> cat output |>
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> cat output |>
: |> echo foo > %o |> output
HERE
tup touch Tupfile
update_fail_msg "Missing input dependency"

cat > Tupfile << HERE
: output |> cat output |>
: |> echo foo > %o |> output
HERE
tup touch Tupfile
update_fail_msg "Explicitly named file 'output' in subdir 1 is scheduled to be deleted"

cat > Tupfile << HERE
: |> echo foo > %o |> output
: output |> cat output |>
HERE
tup touch Tupfile
update
