#! /bin/sh -e

# Duplicate inputs should be pruned.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} foo.o |> gcc %f -o %o |> prog
HERE
echo 'int main(void) {return 0;}' > foo.c
tup touch foo.c Tupfile
update

tup_object_exist . 'gcc foo.o -o prog'

eotup
