#! /bin/sh -e

# Make sure we can remove an explicit dependency.

. ../tup.sh
cat > Tupfile << HERE
: foo.c | bar.h |> cat %f |>
HERE
tup touch foo.c bar.h Tupfile
tup parse

tup_dep_exist . foo.c . 'cat foo.c'
tup_dep_exist . bar.h . 'cat foo.c'

cat > Tupfile << HERE
: foo.c |> cat %f |>
HERE
tup touch Tupfile
tup parse
tup_dep_exist . foo.c . 'cat foo.c'
tup_dep_no_exist . bar.h . 'cat foo.c'
