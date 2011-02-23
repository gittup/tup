#! /bin/sh -e

# This tries to do an fopen inside a constructor of a shared library linked
# into the main executable. Apparently this is what libselinux.so.1 does on
# the Ubuntu machine I'm using to test 64-bit, and it's linked into a bunch
# of utilities like cp and mv. This breaks because apparently my constructor
# in ldpreload isn't called until after all shared libraries are loaded.

. ./tup.sh
check_no_windows shlib
cat > Tupfile << HERE
: lib.c |> gcc -fPIC -shared %f -o %o |> lib.so
: prog.c lib.so |> gcc %f -o %o |> prog
: prog |> LD_LIBRARY_PATH=. ./%f |>
HERE
cat > lib.c << HERE
#include <stdio.h>
/* Constructor! The burninator! */
static void my_init(void) __attribute__((constructor));
static void my_init(void) {fopen("foo.txt", "r");}
HERE
cat > prog.c << HERE
int main(void) {return 0;}
HERE
tup touch Tupfile foo.txt lib.c prog.c
update

eotup
