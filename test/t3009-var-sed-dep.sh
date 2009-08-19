#! /bin/sh -e

# Make sure a var/sed command ends up propagating down the DAG. When I made the
# sticky link change, it looks like this broke.

. ../tup.sh
cat > Tupfile << HERE
, foo.txt |> out.txt
: out.txt |> cat %f > %o |> new.txt
HERE
echo "hey @CONFIG_FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
tup varsetall CONFIG_FOO=sup
update
tup_object_exist . foo.txt out.txt new.txt
(echo "hey sup yo") | diff out.txt -
(echo "hey sup yo") | diff new.txt -

echo "a @CONFIG_FOO@ b" > foo.txt
tup touch foo.txt
update
(echo "a sup b") | diff out.txt -
(echo "a sup b") | diff new.txt -
