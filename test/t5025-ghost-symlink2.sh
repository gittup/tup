#! /bin/sh -e

# See what happens if we have a valid symlink, then remove the destination
# node, and then re-create the destination.

. ../tup.sh
echo "#define FOO 3" > foo-x86.h
ln -s foo-x86.h foo.h
cat > Tupfile << HERE
: foo.h |> (cat %f || echo 'nofile') > %o |> output.txt
HERE
tup touch foo-x86.h foo.h
update
echo '#define FOO 3' | diff - output.txt
check_updates foo.h output.txt
check_updates foo-x86.h output.txt

rm -f foo-x86.h
tup rm foo-x86.h
update
echo 'nofile' | diff - output.txt
# Careful: Can't do check_updates with foo.h here since the touch() will end
# up changing the sym field of foo.h

echo "#define FOO new" > foo-x86.h
tup touch foo-x86.h
update
echo '#define FOO new' | diff - output.txt
check_updates foo.h output.txt
check_updates foo-x86.h output.txt
