#! /bin/sh -e

# Try to use a %-flag inside the order-only prereqs in a !-macro.

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach | %B.h |> gcc -c %f -o %o |> %B.o

: |> echo '#define FOO 3' > %o |> foo.h
: foo.c |> !cc |>
HERE
tup touch foo.c Tupfile
update

tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'

eotup
