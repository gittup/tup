#! /bin/sh -e

# If we create a symlink to somewhere that doesn't exist, we should get a ghost
# node there, and properly update things if the file is later created.

. ./tup.sh
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

# Now point foo.h to a non-existent foo-ppc.h. The command is structured so
# that the update will still succeed without the symlink working, so output.txt
# should be 'nofile' now.
ln -sf foo-ppc.h foo.h
tup touch foo.h
update
echo 'nofile' | diff - output.txt

# Now all we do is create the content at the symlink. The cat command should
# already have a dependency on foo-ppc.h, so the update should end up copying
# the contents of foo-ppc.h into output.txt.
echo "#define FOO 4" > foo-ppc.h
tup touch foo-ppc.h
update
echo '#define FOO 4' | diff - output.txt
check_updates foo.h output.txt
check_no_updates foo-x86.h output.txt
check_updates foo-ppc.h output.txt

eotup
