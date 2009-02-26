#! /bin/sh -e

# Test the TUP_TOP variable and include_root keywords

. ../tup.sh
mkdir fs
cat > fs/Tupfile << HERE
include_root bar/Install.tup
include_root tab/Install.tup
: foreach \$(lib) |> cp %f %o |> %b
HERE
mkdir bar
cat > bar/Install.tup << HERE
lib += \$(TUP_TOP)/bar/foo.so
HERE

mkdir tab
cat > tab/Install.tup << HERE
lib += \$(TUP_TOP)/tab/blah.so
HERE

touch bar/foo.so
touch tab/blah.so

tup touch fs/Tupfile bar/Install.tup tab/Install.tup bar/foo.so tab/blah.so
update
tup_object_exist fs foo.so blah.so
check_exist fs/foo.so fs/blah.so
