#! /bin/sh -e

# Make sure that when we use order-only dependencies for an auto-generated
# header, that only the C files that actually include the header are
# re-compiled after the first build.
. ./tup.sh
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE

echo "#define FOO 3" > foo.h.in
cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
cat > bar.c << HERE
int bar(void) {return 7;}
HERE
tup touch bar.c foo.c foo.h.in Tupfile
update

tup_object_exist . foo.o bar.o
check_exist foo.o bar.o

# Remove the object files behind tup's back, make sure only foo.o is
# re-created.
rm foo.o bar.o

tup touch foo.h.in
update --no-scan
check_exist foo.o
check_not_exist bar.o

# Now if we include foo.h, we should be re-generated
cat > bar.c << HERE
#include "foo.h"
int bar(void) {return FOO;}
HERE
tup touch bar.c
update --no-scan
check_exist bar.o

rm foo.o bar.o
tup touch foo.h.in
update --no-scan
check_exist foo.o bar.o

eotup
