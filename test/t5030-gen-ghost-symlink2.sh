#! /bin/sh -e

# Try to generate a ghost symlink in a subdir from a rule.

. ../tup.sh
cat > Tupfile << HERE
: |> rm -f %o; ln -s secret/ghost %o |> foo
HERE
tup touch Tupfile
update
tup_object_exist . foo secret
tup_object_exist secret ghost

rm -f Tupfile
tup rm Tupfile
update
tup_object_no_exist secret ghost
tup_object_no_exist . foo secret
