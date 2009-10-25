#! /bin/sh -e

# See if we can remove the generated header and the filename from the rule at
# the same time. This should work because the generated header will be marked
# deleted, so we shouldn't have to worry about it in the rule. (Of course if it
# was still included in the C file the compiler would whine about it since we
# have to run the command again anyway).
. ../tup.sh
tup config num_jobs 1
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE

echo "#define FOO 3" > foo.h.in
cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
tup touch foo.c foo.h.in Tupfile
update

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cat > foo.c << HERE
int main(void) {return 7;}
HERE
tup touch Tupfile foo.c
update
