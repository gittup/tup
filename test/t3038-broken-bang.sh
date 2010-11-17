#! /bin/sh -e

# Make sure an invalid !-macro doesn't segfault

. ./tup.sh
# Note: No '=' sign
cat > Tupfile << HERE
!cc | foo.h |> gcc -c %f -o %o |>
HERE
tup touch Tupfile
update_fail_msg "Expecting '=' to set the bang rule"

eotup
