#! /bin/sh -e

# Test using the --binary flag to tup varsed

. ./tup.sh
check_no_windows varsed
cat > Tupfile << HERE
: foo.txt |> tup varsed --binary %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt
echo "hey sup yo" | diff out.txt -

varsetall FOO=y
update
echo "hey 1 yo" | diff out.txt -

varsetall FOO=n
update
echo "hey 0 yo" | diff out.txt -

eotup
