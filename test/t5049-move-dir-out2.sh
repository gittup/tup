#! /bin/sh -e

# Try to move a directory out of tup, and make sure dependent Tupfiles are
# parsed.

. ./tup.sh
mkdir real
cd real
re_init
tmkdir sub
echo ': |> echo blah |>' > sub/Test.tup
echo 'include sub/Test.tup' > Tupfile
tup touch Tupfile sub/Test.tup
update

mv sub ..
tup rm sub
update_fail

eotup
