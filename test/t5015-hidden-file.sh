#! /bin/sh -e

# Make sure hidden files are ignored. I don't really remember why I do this,
# but I guess it makes sense not to look at things like .git, .svn, etc. The
# reason we don't want it to show up from the ld_preload wrapper is because
# normally the hidden files won't be in the .tup/db since the monitor won't
# put them there.

. ../tup.sh
cat > Tupfile << HERE
: |> cat .hidden |>
: |> cat yo/.hidden_dir/foo |>
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
update
tup_dep_no_exist . .hidden . 'cat .hidden'
tup_dep_no_exist yo/.hidden_dir foo . 'cat yo/.hidden_dir/foo'
