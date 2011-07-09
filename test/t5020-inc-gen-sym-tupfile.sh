#! /bin/sh -e

# Make sure we can't include a Tupfile by using a generated symlink. If we
# could, we'd have to go from the update phase back to the create phase, which
# would be silly. Let's *not* do the time warp again.

. ./tup.sh
check_no_windows symlink

# Make the symlink first, in a separate directory. That way it will exist
# and not be marked delete when we create a new Tupfile in the top-level
tmkdir foo
echo 'var = 3' > foo/x86.tup
cat > foo/Tupfile << HERE
: x86.tup |> ln -s %f %o |> arch.tup
HERE
tup touch foo/x86.tup foo/Tupfile
update

cat > Tupfile << HERE
include foo/arch.tup
HERE
tup touch Tupfile
update_fail_msg "Unable to include generated file"

eotup
