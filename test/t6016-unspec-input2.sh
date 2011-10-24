#! /bin/sh -e

# Try to include a header that was auto-generated from another command. Assume
# we correctly have the dependency specified in the rule the first time, and
# then later remove it. We should get yelled at.
. ./tup.sh
single_threaded
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
: foo.h.in |> cp %f %o |> %B
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
update_fail_msg "Missing input dependency"

eotup
