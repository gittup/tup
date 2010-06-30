#! /bin/sh -e

# Make sure we can't use a bin that wasn't specified.

. ./tup.sh
cat > Tupfile << HERE
: {objs} |> gcc %f -o %o |> prog
HERE
tup touch Tupfile
parse_fail_msg "Unable to find bin 'objs'"

eotup
