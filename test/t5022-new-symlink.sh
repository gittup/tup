#! /bin/sh -e

# Make sure when a symlink is created, the directory gets re-parsed.

. ../tup.sh

echo 'int foo(void) {return 0;}' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c Tupfile
update
check_exist foo.o

ln -s foo.c bar.c
tup touch bar.c
update
check_exist bar.o
