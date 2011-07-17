#! /bin/sh -e

# Make sure a generated file can become a normal file while being used as an
# input.

. ./tup.sh

cat > Tupfile << HERE
: |> echo generated > %o |> genfile.txt
: genfile.txt |> cat %f > %o |> output.txt
HERE
tup touch Tupfile
update

echo 'generated' | diff - output.txt

cat > Tupfile << HERE
: genfile.txt |> cat %f > %o |> output.txt
HERE
echo 'manual' > genfile.txt
tup touch genfile.txt Tupfile
update

echo 'manual' | diff - output.txt

eotup
