#! /bin/sh -e

# Specify a header as an auto dependency, but the C file doesn't actually
# include it. The update will cause the link to go away. Then if we change the
# C file to include the header (without changing the Tupfile), we get yelled at
# even though the dependency is specified in the Tupfile. This dependency
# should stick around somehow.
. ../tup.sh
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c | foo.h |> gcc -c %f -o %o |> %F.o
HERE

echo "#define FOO 3" > foo.h.in
cat > foo.c << HERE
#define FOO 4
int main(void) {return FOO;}
HERE
tup touch foo.c foo.h.in Tupfile
update

cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
tup touch foo.c
update
