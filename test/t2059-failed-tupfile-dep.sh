#! /bin/sh -e

# Make sure a dependent Tupfile that fails still gets re-parsed.

. ./tup.sh
tmkdir foo
tmkdir bar
cat > foo/Tupfile << HERE
: |> echo yo > %o |> yo.h
HERE
cat > bar/Tupfile << HERE
: ../foo/*.txt |> cp %f %o |> output
: ../foo/*.c |> cp %f %o |> output
HERE
tup touch foo/Tupfile bar/Tupfile foo/ok.txt
update

# Change yo.h to yo.c so the second rule in bar/Tupfile is triggered to also
# hit the same output file (thus causing an error).
cat > foo/Tupfile << HERE
: |> echo yo > %o |> yo.c
HERE
tup touch foo/Tupfile
update_fail

# An update should again try to parse bar and fail again.
update_fail

eotup
