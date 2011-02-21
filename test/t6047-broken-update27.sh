#! /bin/sh -e

# Similar to t6046, but this now adds another command into the mix. The point
# of this test is to check for a longer chain in a circular dependency. In
# other words, the fix for t6046 can't simply do a 'foreach output tupid {remove
# link (tupid -> cmdid)}'.

. ./tup.sh
cat > Tupfile << HERE
: |> cat foo 2>/dev/null || true; touch bar |> bar
: bar |> cat bar 2>/dev/null; touch foo |>
HERE
tup touch Tupfile
update_fail_msg "Unspecified output files"

cat > Tupfile << HERE
: |> cat foo 2>/dev/null || true; touch bar |> bar
: bar |> cat bar 2>/dev/null; touch foo |> foo
HERE
tup touch Tupfile
update_fail_msg "Missing input dependency"

eotup
