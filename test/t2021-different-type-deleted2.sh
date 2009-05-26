#! /bin/sh -e

# See what happens if we create a directory, then delete it, then re-create it
# as a file before running update.

. ../tup.sh

tmkdir foo
touch foo/a
tup touch foo/a
rm foo/a
tup rm foo/a
rmdir foo
tup rm foo

touch foo
tup touch foo
