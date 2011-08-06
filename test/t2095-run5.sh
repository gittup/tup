#! /bin/sh -e

# Try to readdir() from a run-script on a subdirectory.
. ./tup.sh

cat > Tupfile << HERE
run sh -e ok.sh
HERE
cat > ok.sh << HERE
ls sub
for i in sub/*.[co]; do
	echo ": |> echo \$i |>"
done
HERE

tmkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile ok.sh sub/foo.c sub/bar.c
update_fail_msg 'Unable to readdir.*sub'

# TODO: Allow readdir() to parse subdirs automatically? Would cause a loop
# in fuse, so multi-threaded fuse may be required.
eotup #TODO

tup_object_exist . 'echo sub/foo.c'
tup_object_exist . 'echo sub/bar.c'
tup_object_exist . 'echo sub/foo.o'
tup_object_exist . 'echo sub/bar.o'

eotup
