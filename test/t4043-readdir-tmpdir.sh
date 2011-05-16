#! /bin/sh -e

# Same as t4041/t4042, but in a subdirectory with tmp directories.

. ./tup.sh

tmkdir sub
tmkdir sub/dir2
touch sub/dir2/a1.txt
touch sub/dir2/a2.txt
cd sub
cat > ok.sh << HERE
mkdir tmpsub
echo sup > tmpsub/baz.txt
echo hey > foo.txt
echo yo > bar.txt
ls *.txt dir2/*.txt tmpsub/*.txt
rm foo.txt bar.txt
rm tmpsub/baz.txt
rmdir tmpsub
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
cd ..
update

for i in foo.txt bar.txt tmpsub/baz.txt dir2/a1.txt dir2/a2.txt; do
	if ! grep "$1" sub/output.dat > /dev/null; then
		echo "Error: '$1' should be in the output file" 1>&2
		exit 1
	fi
done

eotup
