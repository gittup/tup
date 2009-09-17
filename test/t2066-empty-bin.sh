#! /bin/sh -e

# See if we can issue a command if a bin is empty.

. ../tup.sh
cat > Tupfile << HERE
obj-@(FOO) += foo.c
: foreach \$(obj-y) |> gcc -c %f -o %o |> %B.o {objs}

!ld = |> gcc -Wl,-r %f -o %o |>
!ld.EMPTY = |> rm -f %o; ar crs %o |>
: {objs} |> !ld |> built-in.o
HERE
tup touch foo.c Tupfile
tup varsetall FOO=y
tup parse
tup_dep_exist . 'foo.c' . 'gcc -c foo.c -o foo.o'
tup_dep_exist . 'foo.o' . 'gcc -Wl,-r foo.o -o built-in.o'
tup_object_no_exist . 'rm -f built-in.o; ar crs built-in.o'

tup varsetall FOO=n
tup parse
tup_dep_no_exist . 'foo.c' . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -Wl,-r foo.o -o built-in.o'
tup_object_exist . 'rm -f built-in.o; ar crs built-in.o'
