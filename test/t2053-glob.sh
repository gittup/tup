#! /bin/sh -e

# Make sure all forms of sqlite globbing work.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c [123].d ?.e |> test %f |>
HERE
tup touch foo.c bar.c 1.d 2.d 3.d 5.e Tupfile
tup touch boo.cc 4.d 52.e
tup parse
for i in foo.c bar.c 1.d 2.d 3.d 5.e; do
	check_exist $i
	tup_dep_exist . $i . "test $i"
done
for i in boo.cc 4.d 52.e; do
	check_exist $i
	tup_dep_no_exist . $i . "test $i"
done

eotup
