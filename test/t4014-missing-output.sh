#! /bin/sh -e

# If a rule lists an output file but doesn't actually create the file, the
# dependency can be lost. This can cause an orphaned output file (if the file
# was previously created by a different command).

. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "int main(void) {}" > foo.c
tup touch foo.c Tupfile
update
check_exist foo.o

cat > Tupfile << HERE
: foreach *.c |> echo gcc -c %f -o %o |> %B.o
HERE

tup touch Tupfile
update_fail_msg "Expected to write to file 'foo.o'"

eotup
