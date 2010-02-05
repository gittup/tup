#! /bin/sh -e

# See if we can get a dependency on both the symlink and the file it points to.

. ./tup.sh
echo "#define FOO 3" > foo-x86.h
echo "#define FOO 4" > foo-ppc.h
ln -s foo-x86.h foo.h
echo '#include "foo.h"' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c foo-x86.h foo-ppc.h foo.h
update
check_exist foo.o

check_updates foo.h foo.o
check_updates foo-x86.h foo.o
check_no_updates foo-ppc.h foo.o

ln -sf foo-ppc.h foo.h
tup touch foo.h
update
check_updates foo.h foo.o
check_no_updates foo-x86.h foo.o
check_updates foo-ppc.h foo.o

eotup
