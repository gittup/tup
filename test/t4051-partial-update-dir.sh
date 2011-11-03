#! /bin/sh -e

# Try to 'tup upd subdir/' and have it update everything in that directory that
# needs to be updated.

. ./tup.sh

tmkdir subdir
tmkdir notupdated

cat > subdir/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cp subdir/Tupfile notupdated/Tupfile

echo 'void foo(void) {}' > subdir/foo.c
echo 'void bar(void) {}' > subdir/bar.c
echo 'void baz(void) {}' > notupdated/baz.c
update_partial subdir

check_exist subdir/foo.o
check_exist subdir/bar.o
check_not_exist notupdated/baz.o

update
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist notupdated/baz.o

eotup
