#! /bin/sh -e

# Duplicate outputs should result in an error.

. ../tup.sh
cat > Tupfile << HERE
: |> touch %o |> bar bar
HERE
tup touch Tupfile
update_fail_msg "multiple commands trying to create file"

cat > Tupfile << HERE
: |> touch %o |> bar
HERE
tup touch Tupfile

update
