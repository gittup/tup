#! /bin/sh -e

# Check that static binning with a varsed rule works.

. ./tup.sh
cat > Tupfile << HERE
, foo.txt |> out.txt {txt}
: foreach {txt} |> cp %f %o |> %B.copied
HERE
echo "hey @FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update

echo "hey sup yo" | diff out.copied -
check_not_exist foo.copied

eotup
