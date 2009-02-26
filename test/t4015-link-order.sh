#! /bin/sh -e

# If we specify multiple inputs, make sure they come in the same order in the
# %f list. We check this by making an archive, which has to come after the
# object file in the command line.

. ../tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: lib.o |> rm -f %o; ar cr %o %f |> lib.a
: foo.o lib.a |> gcc %f -o %o |> prog
HERE

echo "int foo(void) {return 3;}" > lib.c
cat > foo.c << HERE
int foo(void);
int main(void) {return foo();}
HERE
tup touch foo.c lib.c Tupfile
update
