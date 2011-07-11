#! /bin/sh -e

# See if we make a ghost node because of a symlink that it gets deleted when
# the broken symlink is removed.

. ./tup.sh
check_no_windows symlink
ln -s ghost foo
cat > Tupfile << HERE
: |> cat foo 2> /dev/null || true |>
HERE
tup touch foo Tupfile
update
tup_object_exist . ghost foo

rm -f foo Tupfile
tup rm foo Tupfile
update
tup_object_no_exist . ghost foo

eotup
