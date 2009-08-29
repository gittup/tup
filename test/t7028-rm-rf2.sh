#! /bin/sh -e

. ../tup.sh
tup monitor
mkdir a
echo ': |> echo "#define FOO 3" > %o |> foo.h' > a/Tupfile
mkdir b
echo ': foreach *.c | ../a/foo.h |> gcc -c %f -o %o |> %B.o' > b/Tupfile
touch b/foo.c
update
tup_dep_exist a foo.h b 'gcc -c foo.c -o foo.o'

rm -rf a
update_fail_msg "Failed to find directory ID for dir"

echo ': foreach *.c |> gcc -c %f -o %o |> %B.o' > b/Tupfile
update
