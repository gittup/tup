#! /bin/sh -e

# Similar to t3009, only make sure that if the variable is deleted the command
# is still executed.

. ./tup.sh
cat > Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
: out.txt |> cat %f > %o |> new.txt
HERE
echo "hey @FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt new.txt
(echo "hey sup yo") | diff out.txt -
(echo "hey sup yo") | diff new.txt -

varsetall
update
(echo "hey  yo") | diff out.txt -
(echo "hey  yo") | diff new.txt -

varsetall FOO=diggity
update
(echo "hey diggity yo") | diff out.txt -
(echo "hey diggity yo") | diff new.txt -

eotup
