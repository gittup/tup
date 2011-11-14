#! /bin/sh -e

# Make sure that updating a directory with no generated files doesn't do
# anything.

. ./tup.sh

tmkdir subdir
tmkdir notupdated
tmkdir docs

cat > subdir/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cp subdir/Tupfile notupdated/Tupfile

echo 'void foo(void) {}' > subdir/foo.c
echo 'void bar(void) {}' > subdir/bar.c
echo 'void baz(void) {}' > notupdated/baz.c
update_partial docs

check_not_exist subdir/foo.o
check_not_exist subdir/bar.o
check_not_exist notupdated/baz.o

update_partial subdir
check_exist subdir/foo.o
check_exist subdir/bar.o
check_not_exist notupdated/baz.o

update
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist notupdated/baz.o

eotup
