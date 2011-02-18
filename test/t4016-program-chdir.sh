#! /bin/sh -e

# Let's see if the wrapper thing will work if we chdir to a new directory, and
# then touch a file back in the original directory. Obviously it doesn't make
# sense to touch a file in the new directory since that would be illegal.

. ./tup.sh

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

tup_dep_exist tmp bar . './ok.sh'
tup_dep_no_exist . bar . './ok.sh'

eotup
