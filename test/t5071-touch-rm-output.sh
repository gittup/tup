#! /bin/sh -e

# Doing touch ok.o; rm -f ok.o ok.c; tup upd shouldn't keep ok.o in the dag

. ./tup.sh

touch ok.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch ok.c Tupfile
update

sleep 1
touch ok.o
tup touch ok.o
rm ok.c ok.o
update
check_not_exist ok.o
tup_object_no_exist . ok.o

eotup
