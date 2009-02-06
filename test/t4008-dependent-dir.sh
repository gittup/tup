#! /bin/sh -e

. ../tup.sh

cat > Tupfile << HERE
: fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE

mkdir fs
cat > fs/Tupfile << HERE
: foreach input/*.o |> cp %f %o |> %b
HERE

mkdir fs/input
cat > fs/input/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
HERE

echo "void ext3fs(void) {}" > fs/input/ext3.c
echo "void ext4fs(void) {}" > fs/input/ext4.c

tup touch Tupfile fs/Tupfile fs/input/Tupfile fs/input/ext3.c fs/input/ext4.c
update

sym_check built-in.o ext3fs ext4fs

echo "void ext5fs(void) {}" > fs/input/ext5.c
tup touch fs/input/ext5.c
update
sym_check built-in.o ext3fs ext4fs ext5fs
