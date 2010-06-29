#! /bin/sh -e

# Make sure the error message when trying to output to a .file is sane.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo > %o |> .bar
HERE
tup touch Tupfile
update_fail_msg "You specified a path '.bar'"

cat > Tupfile << HERE
: |> echo foo > %o |> foo/.bar
HERE
tup touch Tupfile
update_fail_msg "You specified a path 'foo/.bar'"

cat > Tupfile << HERE
: |> echo foo > %o |> foo/.bar/baz
HERE
tup touch Tupfile
update_fail_msg "You specified a path 'foo/.bar/baz'"

eotup
