#! /bin/sh -e

# Test the nochdir and include_root keywords

. ../tup.sh
mkdir fs
cat > fs/Tupfile << HERE
include_root bar/Install.tup
: foreach nochdir \$(lib) |> echo cp %f %o |> %b
HERE
mkdir bar
cat > bar/Install.tup << HERE
lib += bar/foo.so
HERE

tup touch fs/Tupfile bar/Install.tup bar/foo.so
update
tup_object_exist fs foo.so
