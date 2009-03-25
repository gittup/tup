#! /bin/sh -e

# Make sure an empty input variable doesn't generate a rule, but a blank input
# pattern does.

. ../tup.sh
cat > Tupfile << HERE
: foreach \$(srcs) |> nope |> %B.o
: \$(objs) |> not gonna work |> prog
: |> echo foo > %o |> bar
HERE

tup touch Tupfile
tup parse
tup_object_no_exist . "nope"
tup_object_no_exist . "not gonna work"
tup_object_exist . "echo foo > bar"
