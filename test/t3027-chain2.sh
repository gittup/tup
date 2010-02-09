#! /bin/sh -e

# Try to chain two foreach !-macros

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |> %B.o
!nm = foreach |> nm %f > %o |>

*chain = !cc |> !nm

srcs += foo.c
srcs += bar.c
: \$(srcs) |> *chain |> %B.nm
HERE
echo 'int main(void) {return 0;}' > foo.c
echo 'void bar(void) {}' > bar.c
tup touch foo.c bar.c Tupfile
update

tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . foo.o . 'nm foo.o > foo.nm'
tup_dep_exist . bar.c . 'gcc -c bar.c -o bar.o'
tup_dep_exist . bar.o . 'nm bar.o > bar.nm'

eotup
