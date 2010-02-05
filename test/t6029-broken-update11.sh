#! /bin/sh -e

# Make sure a new incoming link deletes the old link. This can happen if the
# command gets marked for deletion because we're reparsing the Tupfile, and
# some other command comes in and snipes the output file.

. ./tup.sh
cat > Tupfile << HERE
: |> echo foo > %o |> output
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> echo bar > %o |> output
: |> echo foo > %o |> output
HERE
tup touch Tupfile
update_fail_msg "Unable to create a unique link"

eotup
