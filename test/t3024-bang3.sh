#! /bin/sh -e

# See if we can set the 'foreach' flag in the !-macro itself

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |>

files += foo.c
files += bar.c
: \$(files) |> !cc |> %B.o
HERE
tup touch foo.c bar.c Tupfile
update

check_exist foo.o bar.o
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.c . 'gcc -c bar.c -o bar.o'

eotup
