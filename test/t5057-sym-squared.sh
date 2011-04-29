#! /bin/sh -e

# Make sure we can make a symlink to a file with a path that also includes
# a symlink.

. ./tup.sh

mkdir foo
ln -s foo boo
touch foo/ok.txt
ln -s boo/ok.txt sym
cat > Tupfile << HERE
: |> cat sym |>
HERE
update
tup_dep_exist . sym . 'cat sym'
tup_dep_exist foo ok.txt . 'cat sym'
tup_dep_exist . boo . 'cat sym'

eotup
