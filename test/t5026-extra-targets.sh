#! /bin/sh -e

# Make sure a command doesn't write to more than it's supposed to.
. ./tup.sh

cat > Tupfile << HERE
: |> echo 'foo' > %o; echo yo > bar |> file1
HERE
tup touch bar Tupfile
update_fail_msg "Unspecified output files"

eotup
