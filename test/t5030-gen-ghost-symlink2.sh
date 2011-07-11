#! /bin/sh -e

# Try to generate a ghost symlink in a subdir from a rule.

. ./tup.sh
check_no_windows symlink
cat > Tupfile << HERE
: |> ln -s secret/ghost %o |> foo
: foo |> cat %f 2>/dev/null || true |>
HERE
tup touch Tupfile
update
tup_object_exist . foo secret

rm -f Tupfile
tup rm Tupfile
update
tup_object_no_exist secret ghost
tup_object_no_exist . foo secret

eotup
