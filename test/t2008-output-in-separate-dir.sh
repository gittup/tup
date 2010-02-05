#! /bin/sh -e

# At the moment I'm making it a failure to output a file in a different
# directory. Since we can input files from different directories properly,
# which seems to be more useful, it would be weird to also output files in a
# different directory.

. ./tup.sh
tmkdir bar
cat > Tupfile << HERE
: |> echo hey %o |> bar/foo.o
HERE
tup touch Tupfile
update_fail
tup_object_no_exist . "echo hey bar/foo.o"
tup_object_no_exist bar "foo.o"

eotup
