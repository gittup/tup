#! /bin/sh -e

# See if we have a ghost node because it is used in a rule and a symlink points
# to it, then delete the symlink. The ghost should hang around because of its
# use in a rule.

. ./tup.sh
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
ln -s ghost foo
tup touch foo ok.sh Tupfile
update
echo nofile | diff output.txt -

rm -f foo
tup rm foo
tup_object_exist . ghost

echo 'alive' > ghost
tup touch ghost
update
echo alive | diff output.txt -

eotup
