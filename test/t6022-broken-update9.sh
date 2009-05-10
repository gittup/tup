#! /bin/sh -e

# This checks for a specific bug where a command would get marked deleted, then
# a new command would try to write to that node and succeed, then the deleted
# command would be re-created and also succeed. The end result is two commands
# trying to write to the same output file, with no error report from tup.

. ../tup.sh
cat > Tupfile << HERE
: |> echo hey > %o |> foo
HERE
tup touch Tupfile
update
tup_dep_exist . 'echo hey > foo' . foo
tup_object_no_exist . 'echo yo > foo'

# Now we make a new command before the first - this should fail to parse,
# resulting in no change to the links.
cat > Tupfile << HERE
: |> echo yo > %o |> foo
: |> echo hey > %o |> foo
HERE
tup touch Tupfile
update_fail
tup_dep_exist . 'echo hey > foo' . foo
tup_object_no_exist . 'echo yo > foo'

# We should still be able to remove the old command and successfully create the
# file with the new command, though.
cat > Tupfile << HERE
: |> echo yo > %o |> foo
HERE
tup touch Tupfile
update
tup_object_no_exist . 'echo hey > foo'
tup_dep_exist . 'echo yo > foo' . foo
