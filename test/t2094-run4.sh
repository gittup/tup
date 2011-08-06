#! /bin/sh -e

# Make sure that we only get regular files and non-deleted generated
# files when doing a readdir().
. ./tup.sh

cat > Tupfile << HERE
run sh ok.sh bad
: foreach *.c |> gcc -c %f -o %o |> %B.o
run sh ok.sh good
HERE
cat > ok.sh << HERE
for i in *.[co]; do
	echo ": |> echo \$1 \$i |>"
done
HERE
tup touch Tupfile ok.sh foo.c bar.c
update

tup_object_exist . 'echo good foo.c'
tup_object_exist . 'echo good bar.c'
tup_object_exist . 'echo good foo.o'
tup_object_exist . 'echo good bar.o'
tup_object_no_exist . 'echo bad foo.o'
tup_object_no_exist . 'echo bad bar.o'

tup touch Tupfile
update
tup_object_exist . 'echo good foo.c'
tup_object_exist . 'echo good bar.c'
tup_object_exist . 'echo good foo.o'
tup_object_exist . 'echo good bar.o'
tup_object_no_exist . 'echo bad foo.o'
tup_object_no_exist . 'echo bad bar.o'

eotup
