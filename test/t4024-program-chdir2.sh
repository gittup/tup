#! /bin/sh -e

# Now try to chdir outside of the tup hierarchy and back in.

. ./tup.sh

mkdir tmp
cd tmp
re_init

tmkdir sub
cd sub
cat > ok.sh << HERE
#! /bin/sh

cd ../..
cd   -   > /dev/null

cat bar > foo
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: |> ./ok.sh |> foo
HERE

echo "yo" > bar

tup touch bar Tupfile ok.sh
update

tup_dep_exist sub bar sub './ok.sh'

eotup
