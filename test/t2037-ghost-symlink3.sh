#! /bin/sh -e

# Make sure that if we have two symlinks pointing to the same ghost, deleting
# one symlink doesn't kill the ghost.

. ./tup.sh
check_no_windows symlink
ln -s ghost foo
ln -s ghost bar
cat > Tupfile << HERE
: foreach foo bar |> cat %f 2>/dev/null || true |>
HERE
tup touch foo bar Tupfile
update
tup_object_exist . ghost foo bar

rm -f foo
tup rm foo
cat > Tupfile << HERE
: bar |> cat %f 2>/dev/null || true |>
HERE
tup touch Tupfile
update
tup_object_no_exist . foo
tup_object_exist . ghost bar

rm -f bar Tupfile
tup rm bar Tupfile
update
tup_object_no_exist . ghost foo bar

eotup
