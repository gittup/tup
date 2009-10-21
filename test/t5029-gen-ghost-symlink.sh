#! /bin/sh -e

# Try to generate a ghost symlink from a rule.

. ../tup.sh
cat > Tupfile << HERE
: |> ln -s ghost %o |> foo
HERE
tup touch Tupfile
update
tup_object_exist . foo ghost
