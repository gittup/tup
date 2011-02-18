#! /bin/sh -e

# The endif here doesn't get parsed and the ifeq statement ends up being false
# through the end. Tup should catch this and report it as an error.

. ./tup.sh
cat > Tupfile << HERE
!cc = |> gcc -c %f -o %o |>
ifeq (1,2)
: a.c |> !cc |> a.o
endif #foo == 2
: b.c |> !cc |> b.o
HERE
tup touch a.c b.c Tupfile
parse_fail_msg "missing endif before EOF"

cat > Tupfile << HERE
!cc = |> gcc -c %f -o %o |>
ifeq (1,2)
: a.c |> !cc |> a.o
endif
: b.c |> !cc |> b.o
HERE
tup touch Tupfile
tup parse
tup_object_exist . "gcc -c b.c -o b.o"

eotup
