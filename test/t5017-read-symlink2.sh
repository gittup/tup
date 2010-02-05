#! /bin/sh -e

# Same as t5016, only the link points to a file in a subdirectory.

. ./tup.sh
tmkdir arch
echo "#define FOO 3" > arch/foo-x86.h
echo "#define FOO 4" > arch/foo-ppc.h
ln -s arch/foo-x86.h foo.h
echo '#include "foo.h"' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c arch/foo-x86.h arch/foo-ppc.h foo.h
update
check_exist foo.o

check_updates foo.h foo.o
check_updates arch/foo-x86.h foo.o
check_no_updates arch/foo-ppc.h foo.o

ln -sf arch/foo-ppc.h foo.h
tup touch foo.h
update
check_updates foo.h foo.o
check_no_updates arch/foo-x86.h foo.o
check_updates arch/foo-ppc.h foo.o

eotup
