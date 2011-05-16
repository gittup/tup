#! /bin/sh -e

# Verify that readdir will correctly read output files. With the mapping
# done in fuse, we need to make sure that readdir contains all the entries
# that will be in the directory.

. ./tup.sh

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
update

if ! grep 'foo.txt' output.dat > /dev/null; then
	echo "Error: 'foo.txt' should be in the output file" 1>&2
	exit 1
fi
if ! grep 'bar.txt' output.dat > /dev/null; then
	echo "Error: 'bar.txt' should be in the output file" 1>&2
	exit 1
fi

eotup
