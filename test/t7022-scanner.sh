#! /bin/sh -e

. ../tup.sh

# Verify that 'tup scan' works as a one-shot monitor. Now 'tup scan' is called
# automatically by 'tup upd' when necessary.
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
echo "int main(void) {return 0;}" > foo.c
tup upd
check_exist foo.o
check_exist prog

# Add new file
echo "void bar(void) {}" > bar.c
tup upd
check_exist bar.o
sym_check prog bar

# Modify file
echo "void bar2(void) {}" >> bar.c
# Same excuse as in t7004
touch -t 202005080000 bar.c
tup upd
sym_check prog bar bar2

# Delete file
rm bar.c
tup upd
sym_check prog ~bar ~bar2

# Modify Tupfile
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch -t 202005080000 Tupfile
tup upd
check_not_exist prog
