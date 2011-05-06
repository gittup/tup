#! /bin/sh -e

# Make sure accessing the same filename in two different tmp directories
# works.

. ./tup.sh

cat > ok.sh << HERE
mkdir tmp1
mkdir tmp2
echo -n foo > tmp1/ok.txt
echo bar > tmp2/ok.txt
cat tmp1/ok.txt tmp2/ok.txt > output.txt
rm tmp1/ok.txt
rm tmp2/ok.txt
rmdir tmp1
rmdir tmp2
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: ok.sh |> ./%f |> output.txt
HERE
tup touch ok.sh Tupfile
update

echo foobar | diff - output.txt

eotup
