#! /bin/sh -e

# Try to chain two foreach !-macros

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |> %B.o
!nm = foreach |> nm %f > %o |>

srcs += foo.c
srcs += bar.c
: \$(srcs) |> !cc |> !nm |> %B.nm
HERE
echo 'int main(void) {return 0;}' > foo.c
echo 'void bar(void) {}' > bar.c
tup touch foo.c bar.c Tupfile
update

eotup
