#! /bin/sh -e

# Try a big command string.
. ./tup.sh

cat > Tupfile << HERE
: *.txt |> ^ Misty cat^ cat %f > %o |> output.txt
HERE
filelist=`seq 1 2000 | awk '{print "hey"$1".txt"}'`
tup touch $filelist
tup touch Tupfile
update

eotup
