#! /bin/sh -e

# Same as t5017, only the symlink points to a directory instead of a file.

. ./tup.sh
check_no_windows symlink
mkdir arch-x86
mkdir arch-ppc
tup touch arch-x86 arch-ppc
echo "#define FOO 3" > arch-x86/foo.h
echo "#define FOO 4" > arch-ppc/foo.h
ln -s arch-x86 arch
echo '#include "arch/foo.h"' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.c arch-x86/foo.h arch-ppc/foo.h arch
update
check_exist foo.o

check_updates arch/foo.h foo.o
check_updates arch-x86/foo.h foo.o
check_no_updates arch-ppc/foo.h foo.o

rm -f arch
ln -sf arch-ppc arch
tup touch arch
update
check_updates arch/foo.h foo.o
check_no_updates arch-x86/foo.h foo.o
check_updates arch-ppc/foo.h foo.o

eotup
