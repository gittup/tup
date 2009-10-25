#! /bin/sh -e

. ../tup.sh
cat > Tupfile << HERE
FOO := 1

srcs := bar.c
ifeq (1,\$(FOO))
srcs += foo.c
endif

: foreach \$(srcs) |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o prog |> prog
HERE

echo "int main(void) {} void bar(void) {}" > bar.c
echo "void foo(void) {}" > foo.c
tup touch foo.c bar.c Tupfile
update
sym_check foo.o foo
sym_check bar.o bar main
sym_check prog foo bar main

cat Tupfile | sed 's/FOO := 1/FOO := 0/' > tmpTupfile
mv tmpTupfile Tupfile
tup touch Tupfile
update

sym_check bar.o bar main
sym_check prog bar main ~foo
check_not_exist foo.o
