#! /bin/sh -e

# Make sure that files that go outside of tup are properly ignored.

. ./tup.sh
check_no_windows shell
tmkdir foo
tmkdir include
cat > foo/Tupfile << HERE
: |> if [ -f ../include/../../../../../../../bar/lg.h ]; then echo yay; else echo no; fi |>
HERE
tup touch foo/Tupfile
update

eotup
