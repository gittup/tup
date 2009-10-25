#! /bin/sh -e

# See that using %[fFbB] when there is no input gets an error.

. ../tup.sh
cat > Tupfile << HERE
: |> cat %f > %o |> bar
HERE
tup touch Tupfile
parse_fail "Shouldn't be able to use %f without input files"

cat > Tupfile << HERE
: |> cat %b > %o |> bar
HERE
tup touch Tupfile
parse_fail "Shouldn't be able to use %b without input files"

cat > Tupfile << HERE
: |> cat %B > %o |> bar
HERE
tup touch Tupfile
parse_fail "Shouldn't be able to use %B without input files"
