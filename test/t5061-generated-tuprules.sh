#! /bin/sh -e

# Make sure we can't sneak in a generated Tuprules.tup file by making it a
# ghost node first and then generating it later.
. ../tup.sh
tmkdir sub
cat > sub/Tupfile << HERE
include_rules
CFLAGS += foo
: |> echo \$(CFLAGS) |>
HERE
tup touch sub/Tupfile
update
tup_object_exist sub 'echo foo'

cat > Tupfile << HERE
: |> echo 'CFLAGS += bar' > %o |> Tuprules.tup
HERE
tup touch Tupfile
update_fail
