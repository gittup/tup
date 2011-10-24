#! /bin/sh -e

# Verify that output does not have the "Active" bar if we are redirecting to
# a file.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo |>
: |> echo bar 1>&2 |>
HERE

tup upd > out.txt
if grep Active out.txt > /dev/null; then
	echo "Error: Shouldn't see 'Active' in the output" 1>&2
	exit 1
fi

eotup
