#! /bin/sh -e

# Apparently if we get through a parsing stage and then remove a file before
# running the command, the output file isn't actually removed from the
# database.
. ../tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
HERE

touch foo.c
touch bar.c
tup touch foo.c bar.c Tupfile
tup parse
rm foo.c
tup rm foo.c
update
tup_object_exist . bar.c bar.o
tup_object_no_exist . foo.c foo.o
