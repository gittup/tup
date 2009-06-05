#! /bin/sh -e

# Test the TUP_CWD variable

. ../tup.sh
tmkdir fs
cat > fs/Tupfile << HERE
include ../bar/Install.tup
include ../tab/Install.tup
: foreach \$(lib) |> cp %f %o |> %b
HERE
tmkdir bar
cat > bar/Install.tup << HERE
lib += \$(TUP_CWD)/foo.so
HERE

tmkdir tab
cat > tab/Install.tup << HERE
lib += \$(TUP_CWD)/blah.so
HERE

touch bar/foo.so
touch tab/blah.so

tup touch fs/Tupfile bar/Install.tup tab/Install.tup bar/foo.so tab/blah.so
update
tup_object_exist fs foo.so blah.so
check_exist fs/foo.so fs/blah.so
