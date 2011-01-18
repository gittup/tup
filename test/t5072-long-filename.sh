#! /bin/sh -e

# As reported on the mailing list, trying to create a file with a name that is
# too long will fail, and then shortening the filename will cause tup to
# get stuck trying to unlink the file.
. ./tup.sh

cat > Tupfile << HERE
: |> echo 1 > %o |> foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
HERE
tup touch Tupfile
update_fail_msg "Unable to unlink previous output file"

cat > Tupfile << HERE
: |> echo 1 > %o |> foo
HERE
tup touch Tupfile
update
check_exist foo

eotup
