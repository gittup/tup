#! /bin/sh -e

# If we have a gcc command like -La -Lb -lfoo, then in theory we depend on both
# a/libfoo.a and b/libfoo.a. However if only b/libfoo.a exists then we need a
# ghost dependency on a/libfoo.a so we can build properly if a/libfoo.a is
# later created.

. ../tup.sh

tmkdir a
tmkdir b

# Verify both files are compiled
echo "void foo(void); int main(void) {foo(); return 0;}" > main.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o | b/libfoo.a |> gcc %f -La -Lb -lfoo -o %o |> prog
: prog |> ./%f > %o |> output.txt
HERE

cat > b/foo.c << HERE
#include <stdio.h>
void foo(void) {printf("libB\n");}
HERE
cat > b/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f |> libfoo.a
HERE
tup touch main.c b/foo.c Tupfile b/Tupfile
update
echo libB | diff - output.txt

# Now if we create a/libfoo.a, the update should fail because the rule in the
# top-level Tupfile doesn't specify a/libfoo.a as a prerequisite. This is the
# part where the ghost nodes come into play - note that here we are only
# touching files in directory a/, so in theory the commands in the top-level
# directory shouldn't need to be re-executed. But because of the linker rule's
# dependency on the ghost a/libfoo.a, the linker command will run again and
# fail (due to the missing input dependency).
cat > a/foo.c << HERE
#include <stdio.h>
void foo(void) {printf("libA\n");}
HERE
cat > a/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f |> libfoo.a
HERE
tup touch a/foo.c a/Tupfile
update_fail

# Add the pre-requisite, and the update should succeed - the program should now
# successfully be linked against a/libfoo.a
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o | a/libfoo.a b/libfoo.a |> gcc %f -La -Lb -lfoo -o %o |> prog
: prog |> ./%f > %o |> output.txt
HERE
tup touch Tupfile
update
echo libA | diff - output.txt
