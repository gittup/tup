#! /bin/sh -e

# If a command creates the correct output file but returns with a failure code,
# then that command is then removed before it ever executes successfully, the
# output file should be removed and not resurrected.
#
# See also t5066.
. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> echo hey > ok.txt; exit 1 |> ok.txt
HERE
tup touch Tupfile
update_fail_msg "failed with return value 1"

cat > Tupfile << HERE
HERE
tup touch Tupfile
update
check_not_exist ok.txt

eotup
