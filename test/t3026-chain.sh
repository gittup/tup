#! /bin/sh -e

# Try to chain !-macros

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |> %B.o
!ld = |> gcc %f -o %o |>

*chain = !cc |> !ld

srcs += foo.c
srcs += bar.c
: \$(srcs) |> *chain |> prog
HERE
echo 'int main(void) {return 0;}' > foo.c
tup touch foo.c bar.c Tupfile
update

check_exist foo.o bar.o prog
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.c . 'gcc -c bar.c -o bar.o'
tup_dep_exist . foo.o . 'gcc foo.o bar.o -o prog'
tup_dep_exist . bar.o . 'gcc foo.o bar.o -o prog'

eotup
