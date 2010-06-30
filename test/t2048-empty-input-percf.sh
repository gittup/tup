#! /bin/sh -e

# See that using %[fbB] when there is no input gets an error.

. ./tup.sh
cat > Tupfile << HERE
: |> cat %f > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%f used in rule pattern and no input files were specified"

cat > Tupfile << HERE
: |> cat %b > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%b used in rule pattern and no input files were specified"

cat > Tupfile << HERE
: |> cat %B > %o |> bar
HERE
tup touch Tupfile
parse_fail_msg "%B used in rule pattern and no input files were specified"

eotup
