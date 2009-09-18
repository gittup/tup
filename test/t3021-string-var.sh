#! /bin/sh -e

# Make sure a var wrapped in quotes has the quotes removed.

. ../tup.sh
cat > Tupfile << HERE
: |> echo @(BAR) |>
HERE
tup touch Tupfile
varsetall BAR='"string"'
update
tup_object_exist . 'echo string'
