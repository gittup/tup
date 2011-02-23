#! /bin/sh -e

# Try extra-outputs, which are like order-only pre-requisites.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: |> echo blah > %o; touch bar |> foo.h | bar
HERE

tup touch Tupfile
tup parse
tup_dep_exist . "echo blah > foo.h; touch bar" . foo.h
tup_dep_exist . "echo blah > foo.h; touch bar" . bar
update

cat > Tupfile << HERE
: |> echo blah > %o; touch bar |> foo.h
HERE
tup touch Tupfile
tup parse

tup_dep_exist . "echo blah > foo.h; touch bar" . foo.h
tup_object_no_exist . bar
update_fail_msg "File 'bar' was written to"

cat > Tupfile << HERE
: |> echo blah > %o; touch bar |> foo.h | bar
HERE
tup touch Tupfile
update

tup_dep_exist . "echo blah > foo.h; touch bar" . foo.h
tup_dep_exist . "echo blah > foo.h; touch bar" . bar

eotup
