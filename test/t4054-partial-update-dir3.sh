#! /bin/sh -e

# Make sure the partial update on a directory is recursive.

. ./tup.sh

tmkdir subdir
tmkdir subdir/extra
tmkdir notupdated

cat > subdir/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cp subdir/Tupfile notupdated/Tupfile
cp subdir/Tupfile subdir/extra/Tupfile

echo 'void foo(void) {}' > subdir/foo.c
echo 'void bar(void) {}' > subdir/bar.c
echo 'void baz(void) {}' > notupdated/baz.c
echo 'void bar(void) {}' > subdir/extra/extra.c

update_partial subdir
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist subdir/extra/extra.o
check_not_exist notupdated/baz.o

update
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist subdir/extra/extra.o
check_exist notupdated/baz.o

eotup
