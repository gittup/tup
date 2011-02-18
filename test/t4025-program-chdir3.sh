#! /bin/sh -e

# Same as t4016, only down in a sub/dir. This just checks some internal details.

. ./tup.sh

tmkdir sub
tmkdir sub/dir
cd sub/dir

tmkdir tmp
cat > ok.sh << HERE
#! /bin/sh
cd tmp
cat bar > ../foo
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: tmp/bar |> ./ok.sh |> foo
HERE

echo "yo" > tmp/bar
echo "not this one" > bar

tup touch bar tmp/bar Tupfile ok.sh
update

tup_dep_exist sub/dir/tmp bar sub/dir './ok.sh'
tup_dep_no_exist sub/dir bar sub/dir './ok.sh'

eotup
