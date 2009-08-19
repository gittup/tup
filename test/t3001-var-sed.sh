#! /bin/sh -e

# Test using a var to sed a file

. ../tup.sh
cat > Tupfile << HERE
, foo.txt |> out.txt
HERE
echo "hey @CONFIG_FOO@ yo" > foo.txt
echo "This is an email@address.com" >> foo.txt
tup touch foo.txt Tupfile
tup varsetall CONFIG_FOO=sup
update
tup_object_exist . foo.txt out.txt
(echo "hey sup yo"; echo "This is an email@address.com") | diff out.txt -

tup varsetall CONFIG_FOO=blah
update
(echo "hey blah yo"; echo "This is an email@address.com") | diff out.txt -
