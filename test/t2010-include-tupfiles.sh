#! /bin/sh -e

# See if we can include multiple Tupfiles

. ../tup.sh
cat > Tupfile << HERE
include foo/Install.tup
include bar/Install.tup
: foreach \$(input) |> echo cp %f %o |> %B.o
HERE

mkdir foo
cat > foo/Install.tup << HERE
input += foo/sball
HERE

mkdir bar
cat > bar/Install.tup << HERE
input += bar/tab
HERE

tup touch foo/Install.tup bar/Install.tup Tupfile foo/sball bar/tab
update
tup_object_exist . sball.o tab.o
