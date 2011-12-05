#! /bin/sh -e

# Try changing PATH - everything should be rebuilt.

. ./tup.sh

tmkdir sub
cat > sub/gcc << HERE
#! /bin/sh
echo hey > foo.o
HERE
chmod +x sub/gcc

cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > foo.c
tup touch foo.c Tupfile sub/gcc
update
sym_check foo.o foo

export PATH=$PWD/sub:$PATH
update

echo "hey" | diff - foo.o

eotup
