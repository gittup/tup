#! /bin/sh -e

# Make sure if we try to read from a ghost using a path with a symlink, we
# get the dependency on the symlink file.

. ./tup.sh
check_no_windows shell

mkdir foo
ln -s foo boo
cat > Tupfile << HERE
: |> if [ -f boo/ghost ]; then cat boo/ghost; else echo nofile; fi > %o |> out.txt
HERE
update
echo 'nofile' | diff - out.txt

mkdir bar
echo 'hey' > bar/ghost
rm boo
ln -s bar boo
tup touch boo
update
echo 'hey' | diff - out.txt

eotup
