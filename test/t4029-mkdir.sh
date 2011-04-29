#! /bin/sh -e

# mkdir&rmdir are currently not supported in a rule.

. ./tup.sh

cat > Tupfile << HERE
: |> mkdir %o |> outdir
HERE
tup touch Tupfile
update_fail_msg "tup error.*mkdir"

tmkdir outdir

cat > Tupfile << HERE
: |> rmdir outdir |>
HERE
tup touch Tupfile
update_fail_msg "tup error.*rmdir"

eotup
