#! /bin/sh -e

# Make sure reading from hidden files causes a problem. Originally I let this
# slip by ok, but I was incorrectly grouping hidden files (eg: .foo) along with
# files that are outside of tup's view (eg: /usr/bin/gcc). Now I should be able
# to give an error on the former while ignoring the latter.

. ../tup.sh
cat > Tupfile << HERE
: |> cat .hidden |>
HERE

echo 'foo' > .hidden
tmkdir yo
mkdir yo/.hidden_dir
if tup touch yo/.hidden_dir; then
	echo 'Error: tup-touching .hidden_dir should be an error' 1>&2
	exit 1
fi
echo 'bar' > yo/.hidden_dir/foo

if tup touch .hidden; then
	echo 'Error: tup-touching .hidden should be an error' 1>&2
	exit 1
fi
if tup touch yo/.hidden_dir/foo; then
	echo 'Error: tup-touching yo/.hidden_dir/foo should be an error' 1>&2
	exit 1
fi
tup touch Tupfile
update_fail
tup_dep_no_exist . .hidden . 'cat .hidden'
