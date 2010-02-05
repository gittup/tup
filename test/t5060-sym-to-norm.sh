#! /bin/sh -e

# Make sure we can have a command that creates a symlink get changed into a
# command that creates a regular file.

. ./tup.sh

cat > Tupfile << HERE
: |> ln -s foo bar |> bar
: bar |> cat %f > %o |> output
HERE
tup touch foo Tupfile
update

cat > Tupfile << HERE
: |> touch bar |> bar
: bar |> cat %f > %o |> output
HERE
tup touch Tupfile
update

# Make sure the sym field in bar no longer points to foo
check_updates bar output
check_no_updates foo output

eotup
