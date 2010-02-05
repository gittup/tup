#! /bin/sh -e

# Make sure a wrong %-flag results in an error.

. ./tup.sh
cat > Tupfile << HERE
: |> echo %Q |>
HERE
tup touch Tupfile
update_fail_msg "Unknown %-flag: 'Q'"

eotup
