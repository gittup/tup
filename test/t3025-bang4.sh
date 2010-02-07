#! /bin/sh -e

# See if we can specify an output pattern in the !-macro, and a different pattern
# in the rule itself.

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |> %B.o

: foo.c |> !cc |>
: bar.c |> !cc |> %B.newo
HERE
tup touch foo.c bar.c Tupfile
update

check_exist foo.o bar.newo
check_not_exist bar.o
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.c . 'gcc -c bar.c -o bar.newo'

eotup
