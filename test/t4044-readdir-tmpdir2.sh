#! /bin/sh -e

# Same as t4043, only two levels of tmp subdirectories.

. ./tup.sh

tmkdir sub
cd sub
cat > ok.sh << HERE
mkdir tmpsub
mkdir tmpsub/level2
echo sup > tmpsub/level2/baz.txt
echo hey > foo.txt
echo yo > bar.txt
# Note: tmpsub/*.txt should not match tmpsub/level2/*.txt
ls *.txt tmpsub/*.txt 2>/dev/null
rm foo.txt bar.txt
rm tmpsub/level2/baz.txt
rmdir tmpsub/level2
rmdir tmpsub
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.dat
HERE
cd ..
update

for i in foo.txt bar.txt; do
	if ! grep "$1" sub/output.dat > /dev/null; then
		echo "Error: '$1' should be in the output file" 1>&2
		exit 1
	fi
done
if grep tmpsub/level2/baz.txt sub/output.dat > /dev/null; then
	echo "Error: tmpsub/level2/baz.txt should not be in the output file yet" 1>&2
	exit 1
fi

cat > ok.sh << HERE
mkdir tmpsub
mkdir tmpsub/level2
echo sup > tmpsub/level2/baz.txt
echo hey > foo.txt
echo yo > bar.txt
# Now we use tmpsub/level2/*.txt
ls *.txt tmpsub/level2/*.txt
rm foo.txt bar.txt
rm tmpsub/level2/baz.txt
rmdir tmpsub/level2
rmdir tmpsub
HERE
tup touch ok.sh
update

for i in foo.txt bar.txt tmpsub/level2/baz.txt; do
	if ! grep "$1" sub/output.dat > /dev/null; then
		echo "Error: '$1' should be in the output file" 1>&2
		exit 1
	fi
done

eotup
