#! /bin/sh -e

# Test ifndef

. ./tup.sh
cat > Tupfile << HERE
ifndef FOO
objs-y += foo.c
endif
: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c Tupfile
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist @ FOO 0 .

varsetall FOO=y
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist @ FOO 0 .

varsetall
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist @ FOO 0 .

varsetall FOO=n
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist @ FOO 0 .

cat > Tupfile << HERE
: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_dep_no_exist @ FOO 0 .

eotup
