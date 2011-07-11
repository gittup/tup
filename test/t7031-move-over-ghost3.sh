#! /bin/sh -e

# We have a ghost node that is pointed to by another node, then move a
# directory over the parent ghost. The rule should execute.
. ./tup.sh
check_monitor_supported
tup monitor

ln -s secret/ghost a
cat > Tupfile << HERE
: |> (cat a 2>/dev/null || echo nofile) > %o |> output.txt
HERE
update
echo nofile | diff - output.txt

mkdir foo
echo hey > foo/ghost
update
echo nofile | diff - output.txt

mv foo secret
update
echo hey | diff - output.txt

eotup
