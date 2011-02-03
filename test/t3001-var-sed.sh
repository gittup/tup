#! /bin/sh -e

# Test using a var to sed a file

. ./tup.sh
cat > Tupfile << HERE
: foo.txt |> tup-varsed %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
echo "This is an email@address.com" >> foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt
(echo "hey sup yo"; echo "This is an email@address.com") | diff out.txt -

varsetall FOO=blah
update
(echo "hey blah yo"; echo "This is an email@address.com") | diff out.txt -

eotup
