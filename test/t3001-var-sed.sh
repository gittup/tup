#! /bin/sh -e

# Test using a var to sed a file

. ../tup.sh
cat > Tupfile << HERE
/ foo.txt |> out.txt
HERE
echo "hey @CONFIG_FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
tup varset CONFIG_FOO sup
tup upd
tup_object_exist . foo.txt out.txt
echo "hey sup yo" | diff out.txt -

tup varset CONFIG_FOO blah
tup upd
echo "hey blah yo" | diff out.txt -
