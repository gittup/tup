#! /bin/sh -e

# Make sure an empty environment variable results in no export.

. ./tup.sh

cat > Tupfile << HERE
export FOO
: |> sh ok.sh > %o |> out.txt
HERE
cat > ok.sh << HERE
echo "foo is \$FOO"
HERE
update
echo 'foo is ' | diff - out.txt

eotup
