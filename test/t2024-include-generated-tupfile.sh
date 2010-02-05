#! /bin/sh -e

# Make sure we can't include a generated Tupfile. Since the file would be
# written way after it needs to be parsed, we'd have to do some kind of
# reloading thing like make does, which would be silly.
#
# The generated Tupfile has to be done in a separate directory - if it's done
# in the same directory, then it will be marked deleted before the Tupfile is
# parsed, so that would get an error anyway. This test is to specifically check
# that it should error on the fact that it was a generated file.

. ./tup.sh
tmkdir foo
cat > foo/Tupfile << HERE
: |> echo "var=foo" > %o |> inc
HERE
touch Tupfile

tup touch Tupfile foo/Tupfile
update

cat > Tupfile << HERE
include foo/inc
HERE
tup touch Tupfile
update_fail

eotup
