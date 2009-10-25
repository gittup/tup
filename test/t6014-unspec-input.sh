#! /bin/sh -e

# Try to include a header that was auto-generated from another command. It's
# possible based on the ordering of the commands that the header will be
# generated before the file is compiled, so things will appear to work.
# However, if the header isn't specified as a dependency for the compilation
# command then things could break during a parallel build or if things get
# re-ordered later. So, that should be detected and return a failure.
. ../tup.sh
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
HERE

echo "#define FOO 3" > foo.h.in
tup touch foo.h.in Tupfile
update

cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c Tupfile
update_fail
