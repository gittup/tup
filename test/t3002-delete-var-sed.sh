#! /bin/sh -e

# Test using a var to sed a file

. ./tup.sh
check_no_windows varsed
cat > Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
echo "This is an email@address.com" >> foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt
tup_object_exist . "tup varsed foo.txt out.txt"
(echo "hey sup yo"; echo "This is an email@address.com") | diff out.txt -

cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_object_no_exist . out.txt
tup_object_no_exist . "tup varsed foo.txt out.txt"

eotup
