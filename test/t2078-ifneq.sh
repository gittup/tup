#! /bin/sh -e

# Test ifndef

. ./tup.sh
cat > Tupfile << HERE
ifneq (\$(CONFIG_FOO),22)
objs-y += foo.c
endif

ifneq (\$(CONFIG_FOO),23)
objs-y += bar.c
endif

: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c bar.c Tupfile
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall FOO=y
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall FOO=22
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall FOO=23
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -c bar.c -o bar.o'

cat > Tupfile << HERE
: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -c bar.c -o bar.o'

eotup
