#! /bin/sh -e

# Seems I broke re-parsing a Tupfile with a var/sed command

. ../tup.sh
cat > Tupfile << HERE
, foo.txt |> out.txt
HERE
echo "hey @CONFIG_FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
tup varset CONFIG_FOO sup
update
tup_object_exist . foo.txt out.txt
echo "hey sup yo" | diff out.txt -

tup touch Tupfile
update
