#! /bin/sh -e

# The ghost nodes must be creating before the links are written to them, but if
# an error occurs while writing the links (such as due to a missing input
# dependency), then the ghost nodes are never connected. If the command changes
# such that it no longer reads from the ghost, then the ghost will persist.

. ./tup.sh

cat > Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
: |> sh ok.sh |>
HERE
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; fi
cat foo.h
HERE
tup touch Tupfile ok.sh

update_fail

cat > ok.sh << HERE
echo yo
HERE
tup touch ok.sh

update
tup_object_no_exist . ghost

eotup
