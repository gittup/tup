#! /bin/sh -e

# Make sure a chain fails if no intermediate outputs are specified.

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |>
!ld = |> gcc %f -o %o |>

*chain = !cc |> !ld

srcs += foo.c
srcs += bar.c
: \$(srcs) |> *chain |> prog
HERE
echo 'int main(void) {return 0;}' > foo.c
tup touch foo.c bar.c Tupfile
update_fail_msg "Intermediate !-macro.*does not specify an output pattern"

eotup
