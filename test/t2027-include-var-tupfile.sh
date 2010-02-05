#! /bin/sh -e

# See if we can include a Tupfile using a variable de-reference.

. ./tup.sh
cat > Tupfile << HERE
var = foo
include \$(var).tup
HERE

cat > foo.tup << HERE
: |> echo foo > %o |> file
HERE
cat > bar.tup << HERE
: |> echo bar > %o |> file
HERE

tup touch Tupfile foo.tup bar.tup
tup parse
tup_object_exist . 'echo foo > file'
tup_dep_exist . foo.tup 0 .
tup_dep_no_exist . bar.tup 0 .

cat > Tupfile << HERE
var = bar
include \$(var).tup
HERE

tup touch Tupfile
tup parse
tup_object_exist . 'echo bar > file'
tup_dep_no_exist . foo.tup 0 .
tup_dep_exist . bar.tup 0 .

eotup
