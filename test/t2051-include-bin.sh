#! /bin/sh -e

# Make sure we can't try to include files from a bin, since those are always
# generated.

. ./tup.sh

cat > Tupfile << HERE
: |> echo "var=foo" > %o |> inc {includes}
include {includes}
HERE
tup touch Tupfile
parse_fail_msg "Failed to parse included file '{includes}'"

eotup
