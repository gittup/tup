#! /bin/sh -e

# Make sure we can't create hidden file nodes, or get dependencies from them
# in commands. The commands should still work to allow things like git describe
# to function, but in general reading from hidden files is discouraged.

. ./tup.sh
cat > Tupfile << HERE
: |> cat .hidden |>
HERE

echo 'foo' > .hidden
tmkdir yo
mkdir yo/.hidden_dir
if tup touch yo/.hidden_dir 2>/dev/null; then
	echo 'Error: tup-touching .hidden_dir should be an error' 1>&2
	exit 1
fi
echo 'bar' > yo/.hidden_dir/foo

if tup touch .hidden 2>/dev/null; then
	echo 'Error: tup-touching .hidden should be an error' 1>&2
	exit 1
fi
if tup touch yo/.hidden_dir/foo 2>/dev/null; then
	echo 'Error: tup-touching yo/.hidden_dir/foo should be an error' 1>&2
	exit 1
fi
tup touch Tupfile
update
tup_object_no_exist . .hidden
tup_object_exist . 'cat .hidden'

eotup
