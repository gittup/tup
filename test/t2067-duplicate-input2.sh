#! /bin/sh -e

# More duplicate input tests.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c foo.c | foo.c foo.h foo.h |> gcc -c %f -o %o |> %B.o {objs}
: {objs} foo.o |> gcc %f -o %o |> prog
: foo.c bar.c foo.c |> echo blah1 %f |>
: bar.c foo.c bar.c |> echo blah2 %f |>
HERE
echo 'int main(void) {return 0;}' > foo.c
tup touch foo.c foo.h bar.c Tupfile
update

tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc foo.o -o prog'
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'echo blah1 foo.c bar.c'
tup_object_exist . 'echo blah2 bar.c foo.c'

eotup
