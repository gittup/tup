#! /bin/sh -e

# Include a separate Tupfile, then stop including it and make sure the link to
# the directory goes away. I put everything in a tmp directory because
# tup_dep_exist doesn't work using '. .' (since I need to use 0 as the parent
# for '.' and I don't feel like fixing it...and I realized all these ''s and
# .'s look like faces.)

. ../tup.sh
mkdir tmp
cat > tmp/Tupfile << HERE
include Tupfile.vars
: foreach *.c |> gcc -c %f -o %o |> %F.o
: *.o |> gcc -o prog %f |> prog
HERE

cat > tmp/Tupfile.vars << HERE
VAR = yo
HERE

echo "int main(void) {return 0;}" > tmp/foo.c
touch tmp/bar.c
tup touch tmp/foo.c tmp/bar.c tmp/Tupfile tmp/Tupfile.vars
update
tup_object_exist tmp foo.c bar.c
tup_object_exist tmp "gcc -c foo.c -o foo.o"
tup_object_exist tmp "gcc -c bar.c -o bar.o"
tup_object_exist tmp "gcc -o prog foo.o bar.o"
tup_dep_exist tmp Tupfile.vars . tmp

cat > tmp/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
: *.o |> gcc -o prog %f |> prog
HERE
tup touch tmp/Tupfile
update
tup_dep_no_exist tmp Tupfile.vars . tmp
