#! /bin/sh -e

# Make sure when moving a directory, that any Tupfiles in that directory cause
# the dependent Tupfiles to be re-parsed. In this case, the top-level Tupfile
# should be re-parsed and fail because a/a2/Test.tup no longer exists.
. ./tup.sh
tmkdir a
tmkdir a/a2
echo 'x = 5' > a/a2/Test.tup
echo 'include a/a2/Test.tup' > Tupfile

tup touch a/a2/Test.tup Tupfile
update

mv a b
tup rm a
tup touch b b/a2 b/a2/Test.tup
update_fail

echo 'include b/a2/Test.tup' > Tupfile
tup touch Tupfile
update

eotup
