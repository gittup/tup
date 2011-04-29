#! /bin/sh -e

# Fifos are currently not allowed.

. ./tup.sh

cat > Tupfile << HERE
: |> mkfifo %o |> testfifo
HERE
tup touch Tupfile
update_fail_msg "testfifo.*Operation not permitted"

eotup
