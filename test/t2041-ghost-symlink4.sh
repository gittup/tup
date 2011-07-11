#! /bin/sh -e

# Make a symlink to a ghost file in a ghost directory.
. ./tup.sh
check_no_windows symlink
ln -s spooky/ghost foo
cat > Tupfile << HERE
: |> cat foo 2>/dev/null || true |>
HERE
tup touch foo Tupfile
update
tup_object_exist . spooky foo

rm -f foo Tupfile
tup rm foo Tupfile
update
tup_object_no_exist . foo
tup_object_no_exist spooky ghost
tup_object_no_exist . spooky

eotup
