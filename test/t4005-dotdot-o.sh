#! /bin/sh -e

. ../tup.sh

cat > Tupfile << HERE
: foreach *.c >> gcc -c %f -o %F.o >> %F.o
: *.o >> ld -r %f -o built-in.o >> built-in.o
HERE

mkdir fs
cat > fs/Tupfile << HERE
: foreach *.c >> gcc -c %f -o %F.o >> %F.o
: *.o ../built-in.o >> gcc %f -o prog >> prog
HERE

echo "int main(void) {return 0;}" > fs/main.c
echo "void ext3fs(void) {}" > ext3.c
echo "void ext4fs(void) {}" > ext4.c

tup touch Tupfile ext3.c ext4.c fs/main.c fs/Tupfile
update

sym_check fs/prog main ext3fs ext4fs
