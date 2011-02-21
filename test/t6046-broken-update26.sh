#! /bin/sh -e

# When upgrading a ghost to a generated file, the ghost still points to all
# the same commands that it used to. This includes the same command that could
# now be writing the file, which causes a circular dependency.

. ./tup.sh
cat > Tupfile << HERE
: |> cat foo 2> /dev/null || true; touch foo |>
HERE
tup touch Tupfile
update_fail_msg "File.*foo.*was written to"

cat > Tupfile << HERE
: |> cat foo 2> /dev/null || true; touch foo |> foo
HERE
tup touch Tupfile
update

eotup
