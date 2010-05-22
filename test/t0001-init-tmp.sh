#! /bin/sh -e

# Make sure 'tup init' works without the --force flag that is used by default
# for these test cases.

tmpdir="/tmp/tup-t0001"
cleanup()
{
	cd /tmp
	rm -rf $tmpdir
}

trap cleanup INT TERM
cleanup
mkdir $tmpdir
cd $tmpdir
tup init
for i in db object shared tri vardict; do
	if [ ! -f ".tup/$i" ]; then
		echo ".tup/$i not created!" 1>&2
		cleanup
		exit 1
	fi
done
cleanup
