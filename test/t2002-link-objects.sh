#! /bin/sh -e

# Since *.o isn't 'tup touched', we have to get them from the output of the
# first rule.

. ../tup.sh
cat > Tupfile << HERE
: foreach *.c >> echo gcc -c %f -o %F.o >> %F.o
: *.o >> echo gcc -o prog %f >> prog
HERE
tup touch foo.c bar.c Tupfile
tup upd
tup_object_exist . foo.c bar.c
tup_object_exist . "echo gcc -c foo.c -o foo.o"
tup_object_exist . "echo gcc -c bar.c -o bar.o"
tup_object_exist . "echo gcc -o prog foo.o bar.o"
