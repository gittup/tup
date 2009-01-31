#! /bin/sh -e

# Make sure that the link from one Tupfile to another stays even if the first
# Tupfile has errors. This could be an issue based on the way I plan to delete
# links before parsing for 4009. I think the rollback in the updater will make
# this all magically work though.
. ../tup.sh

mkdir bar
cat > bar/Tupfile << HERE
: ../fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE

mkdir fs
cat > fs/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
HERE

echo "void ext3fs(void) {}" > fs/ext3.c
echo "void ext4fs(void) {}" > fs/ext4.c

tup touch bar/Tupfile fs/Tupfile fs/ext3.c fs/ext4.c
update

tup_dep_exist . fs . bar

cat > bar/Tupfile << HERE
bork
: ../fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE
tup touch bar/Tupfile
update_fail
tup_dep_exist . fs . bar
