#! /bin/sh -e

# Same as t5016, only the symlink is generated from a rule.

. ./tup.sh
check_no_windows symlink
echo "#define FOO 3" > foo-x86.h
echo "#define FOO 4" > foo-ppc.h
echo '#include "foo.h"' > foo.c
cat > Tupfile << HERE
ARCH = x86
: foo-\$(ARCH).h |> ln -s %f %o |> foo.h
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c foo-x86.h foo-ppc.h
update
check_exist foo.o

check_updates foo-x86.h foo.o
check_updates foo.h foo.o
check_no_updates foo-ppc.h foo.o

cat > Tupfile << HERE
ARCH = ppc
: foo-\$(ARCH).h |> ln -s %f %o |> foo.h
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
update
check_updates foo-ppc.h foo.o
check_updates foo.h foo.o
check_no_updates foo-x86.h foo.o

eotup
