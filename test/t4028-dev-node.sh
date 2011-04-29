#! /bin/sh -e

# Device nodes are currently not allowed.

. ./tup.sh

cat > Tupfile << HERE
: |> mknod %o c 1 3 |> nulltest
HERE
tup touch Tupfile
update_fail_msg "nulltest.*Operation not permitted"

eotup
