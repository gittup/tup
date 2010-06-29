#! /bin/sh -e

# Make sure that if a command creates a file and exits with a failure code,
# the outputs are still checked.
. ./tup.sh

cat > Tupfile << HERE
: |> echo hey > ok.txt; exit 1 |>
HERE
tup touch Tupfile
update_fail_msg "failed with return value 1"

cat > Tupfile << HERE
HERE
tup touch Tupfile
update
check_not_exist ok.txt

eotup
