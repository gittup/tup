#! /bin/sh -e

# Test to make sure if a Tupfile is dependent on another that the link between
# directories exists, and then when the dependency between Tupfiles is removed,
# the link is removed.
. ../tup.sh

tmkdir bar
cat > bar/Tupfile << HERE
: ../fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE

tmkdir fs
cat > fs/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "void ext3fs(void) {}" > fs/ext3.c
echo "void ext4fs(void) {}" > fs/ext4.c

tup touch bar/Tupfile fs/Tupfile fs/ext3.c fs/ext4.c
update

tup_dep_exist . fs . bar
sym_check bar/built-in.o ext3fs ext4fs

cat > bar/Tupfile << HERE
HERE
tup touch bar/Tupfile
update
tup_dep_no_exist . fs . bar
check_not_exist bar/built-in.o
