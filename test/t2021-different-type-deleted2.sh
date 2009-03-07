#! /bin/sh -e

# See what happens if we create a directory, then delete it, then re-create it
# as a file before running update.

. ../tup.sh

mkdir foo
touch foo/a
tup touch foo/a
rm foo/a
rmdir foo
tup delete foo/a
tup delete foo

touch foo
tup touch foo
