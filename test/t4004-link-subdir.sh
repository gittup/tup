#! /bin/sh -e

. ../tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
: *.o fs/built-in.o |> gcc %f -o prog |> prog
HERE

tmkdir fs
cat > fs/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
: *.o |> ld -r %f -o built-in.o |> built-in.o
HERE

echo "int main(void) {return 0;}" > main.c
echo "void ext3fs(void) {}" > fs/ext3.c
echo "void ext4fs(void) {}" > fs/ext4.c

tup touch Tupfile main.c fs/ext3.c fs/ext4.c fs/Tupfile
update

sym_check prog main ext3fs ext4fs
