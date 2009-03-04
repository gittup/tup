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
mkdir -p yo/.hidden_dir
echo 'bar' > yo/.hidden_dir/foo
tup touch Tupfile .hidden yo/.hidden_dir/foo
update
tup_dep_no_exist . .hidden . 'cat .hidden'
tup_dep_no_exist yo/.hidden_dir foo . 'cat yo/.hidden_dir/foo'
