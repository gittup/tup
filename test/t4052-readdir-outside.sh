#! /bin/sh -e

# Make sure we can readdir() outside the .tup hierarchy during parsing, even if
# we are still inside the fuse filesystem.

. ./tup.sh

touch foo.txt
touch bar.txt

mkdir tmp
cd tmp
re_init

cat > Tupfile << HERE
run sh foo.sh
HERE
cat > foo.sh << HERE
for i in ../*.txt; do
	echo ": |> echo \$i |>"
done
HERE
tup touch foo.sh Tupfile
update

tup_object_exist . 'echo ../foo.txt'
tup_object_exist . 'echo ../bar.txt'

eotup
