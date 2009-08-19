#! /bin/sh -e

# Similar to t3009, only make sure that if the variable is deleted the command
# is still executed.

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

tup varsetall
update_fail

tup varsetall CONFIG_FOO=diggity
update
(echo "hey diggity yo") | diff out.txt -
(echo "hey diggity yo") | diff new.txt -
