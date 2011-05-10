#! /bin/sh -e

# Check to make sure we get the warnings from hidden files.
# (This behavior of tup may be stupid).

. ./tup.sh

cat > Tupfile << HERE
: |> touch .foo; touch .bar |>
HERE
if tup upd 2>&1 | grep "Update resulted in 2 warnings" > /dev/null; then
	:
else
	echo "Error: Expected 2 warnings." 1>&2
	exit 1
fi

eotup
