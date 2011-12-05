#! /bin/sh -e

# Make sure environment variables are ignored

export FOO="hey"
. ./tup.sh

cat > Tupfile << HERE
: |> sh ok.sh > %o |> out.txt
HERE
cat > ok.sh << HERE
echo "foo is \$FOO"
HERE

update
echo 'foo is ' | diff - out.txt

eotup
