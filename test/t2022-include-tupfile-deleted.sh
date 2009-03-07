#! /bin/sh -e

# See if we delete an included Tupfile that we get re-parsed.

. ../tup.sh
cat > Tupfile << HERE
include yo/Install.tup
: |> echo foo |>
HERE

mkdir yo
cat > yo/Install.tup << HERE
: |> echo bar |>
HERE

tup touch yo/Install.tup Tupfile
update
tup_object_exist . 'echo foo'
tup_object_exist . 'echo bar'

rm yo/Install.tup
tup delete yo/Install.tup
rmdir yo
tup delete yo

cat > Tupfile << HERE
: |> echo foo |>
HERE
# Note: Do not 'tup touch Tupfile' - want to see if rming Install.tup causes it
# to be re-parsed. But I also want to parse it successfully so I can see the
# command gets removed.

update
tup_object_exist . 'echo foo'
tup_object_no_exist . 'echo bar'
