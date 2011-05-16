#! /bin/sh -e

# Same as t4041, but in a subdirectory.

. ./tup.sh

tmkdir sub
cd sub
cat > ok.sh << HERE
echo hey > foo.txt
echo yo > bar.txt
ls *.txt
rm foo.txt bar.txt
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
cd ..
update

if ! grep 'foo.txt' sub/output.dat > /dev/null; then
	echo "Error: 'foo.txt' should be in the output file" 1>&2
	exit 1
fi
if ! grep 'bar.txt' sub/output.dat > /dev/null; then
	echo "Error: 'bar.txt' should be in the output file" 1>&2
	exit 1
fi

eotup
