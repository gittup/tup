#! /bin/sh -e

# link(): It's dangerous to go alone! Take this.

. ./tup.sh

cat > Tupfile << HERE
: tmp |> link %f %o |> outlink
HERE
tup touch tmp Tupfile
update_fail_msg "tup error.*hard links"

eotup
